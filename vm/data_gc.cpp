#include "master.hpp"

namespace factor
{

void factorvm::init_data_gc()
{
	performing_gc = false;
	last_code_heap_scan = data->nursery();
	collecting_aging_again = false;
}


/* Given a pointer to oldspace, copy it to newspace */
object *factorvm::copy_untagged_object_impl(object *pointer, cell size)
{
	if(newspace->here + size >= newspace->end)
		longjmp(gc_jmp,1);
	object *newpointer = allot_zone(newspace,size);

	gc_stats *s = &stats[collecting_gen];
	s->object_count++;
	s->bytes_copied += size;

	memcpy(newpointer,pointer,size);
	return newpointer;
}


object *factorvm::copy_object_impl(object *untagged)
{
	object *newpointer = copy_untagged_object_impl(untagged,untagged_object_size(untagged));
	untagged->h.forward_to(newpointer);
	return newpointer;
}


bool factorvm::should_copy_p(object *untagged)
{
	if(in_zone(newspace,untagged))
		return false;
	if(collecting_gen == data->tenured())
		return true;
	else if(data->have_aging_p() && collecting_gen == data->aging())
		return !in_zone(&data->generations[data->tenured()],untagged);
	else if(collecting_gen == data->nursery())
		return in_zone(&nursery,untagged);
	else
	{
		critical_error("Bug in should_copy_p",(cell)untagged);
		return false;
	}
}


/* Follow a chain of forwarding pointers */
object *factorvm::resolve_forwarding(object *untagged)
{
	check_data_pointer(untagged);

	/* is there another forwarding pointer? */
	if(untagged->h.forwarding_pointer_p())
		return resolve_forwarding(untagged->h.forwarding_pointer());
	/* we've found the destination */
	else
	{
		untagged->h.check_header();
		if(should_copy_p(untagged))
			return copy_object_impl(untagged);
		else
			return untagged;
	}
}


template <typename TYPE> TYPE *factorvm::copy_untagged_object(TYPE *untagged)
{
	check_data_pointer(untagged);

	if(untagged->h.forwarding_pointer_p())
		untagged = (TYPE *)resolve_forwarding(untagged->h.forwarding_pointer());
	else
	{
		untagged->h.check_header();
		untagged = (TYPE *)copy_object_impl(untagged);
	}

	return untagged;
}


cell factorvm::copy_object(cell pointer)
{
	return RETAG(copy_untagged_object(untag<object>(pointer)),TAG(pointer));
}


void factorvm::copy_handle(cell *handle)
{
	cell pointer = *handle;

	if(!immediate_p(pointer))
	{
		object *obj = untag<object>(pointer);
		check_data_pointer(obj);
		if(should_copy_p(obj))
			*handle = copy_object(pointer);
	}
}


/* Scan all the objects in the card */
void factorvm::copy_card(card *ptr, cell gen, cell here)
{
	cell card_scan = card_to_addr(ptr) + card_offset(ptr);
	cell card_end = card_to_addr(ptr + 1);

	if(here < card_end)
		card_end = here;

	copy_reachable_objects(card_scan,&card_end);

	cards_scanned++;
}


void factorvm::copy_card_deck(card_deck *deck, cell gen, card mask, card unmask)
{
	card *first_card = deck_to_card(deck);
	card *last_card = deck_to_card(deck + 1);

	cell here = data->generations[gen].here;

	u32 *quad_ptr;
	u32 quad_mask = mask | (mask << 8) | (mask << 16) | (mask << 24);

	for(quad_ptr = (u32 *)first_card; quad_ptr < (u32 *)last_card; quad_ptr++)
	{
		if(*quad_ptr & quad_mask)
		{
			card *ptr = (card *)quad_ptr;

			int card;
			for(card = 0; card < 4; card++)
			{
				if(ptr[card] & mask)
				{
					copy_card(&ptr[card],gen,here);
					ptr[card] &= ~unmask;
				}
			}
		}
	}

	decks_scanned++;
}


/* Copy all newspace objects referenced from marked cards to the destination */
void factorvm::copy_gen_cards(cell gen)
{
	card_deck *first_deck = addr_to_deck(data->generations[gen].start);
	card_deck *last_deck = addr_to_deck(data->generations[gen].end);

	card mask, unmask;

	/* if we are collecting the nursery, we care about old->nursery pointers
	but not old->aging pointers */
	if(collecting_gen == data->nursery())
	{
		mask = card_points_to_nursery;

		/* after the collection, no old->nursery pointers remain
		anywhere, but old->aging pointers might remain in tenured
		space */
		if(gen == data->tenured())
			unmask = card_points_to_nursery;
		/* after the collection, all cards in aging space can be
		cleared */
		else if(data->have_aging_p() && gen == data->aging())
			unmask = card_mark_mask;
		else
		{
			critical_error("bug in copy_gen_cards",gen);
			return;
		}
	}
	/* if we are collecting aging space into tenured space, we care about
	all old->nursery and old->aging pointers. no old->aging pointers can
	remain */
	else if(data->have_aging_p() && collecting_gen == data->aging())
	{
		if(collecting_aging_again)
		{
			mask = card_points_to_aging;
			unmask = card_mark_mask;
		}
		/* after we collect aging space into the aging semispace, no
		old->nursery pointers remain but tenured space might still have
		pointers to aging space. */
		else
		{
			mask = card_points_to_aging;
			unmask = card_points_to_nursery;
		}
	}
	else
	{
		critical_error("bug in copy_gen_cards",gen);
		return;
	}

	card_deck *ptr;

	for(ptr = first_deck; ptr < last_deck; ptr++)
	{
		if(*ptr & mask)
		{
			copy_card_deck(ptr,gen,mask,unmask);
			*ptr &= ~unmask;
		}
	}
}


/* Scan cards in all generations older than the one being collected, copying
old->new references */
void factorvm::copy_cards()
{
	u64 start = current_micros();

	cell i;
	for(i = collecting_gen + 1; i < data->gen_count; i++)
		copy_gen_cards(i);

	card_scan_time += (current_micros() - start);
}


/* Copy all tagged pointers in a range of memory */
void factorvm::copy_stack_elements(segment *region, cell top)
{
	cell ptr = region->start;

	for(; ptr <= top; ptr += sizeof(cell))
		copy_handle((cell*)ptr);
}


void factorvm::copy_registered_locals()
{
	std::vector<cell>::const_iterator iter = gc_locals.begin();
	std::vector<cell>::const_iterator end = gc_locals.end();

	for(; iter < end; iter++)
		copy_handle((cell *)(*iter));
}


void factorvm::copy_registered_bignums()
{
	std::vector<cell>::const_iterator iter = gc_bignums.begin();
	std::vector<cell>::const_iterator end = gc_bignums.end();

	for(; iter < end; iter++)
	{
		bignum **handle = (bignum **)(*iter);
		bignum *pointer = *handle;

		if(pointer)
		{
			check_data_pointer(pointer);
			if(should_copy_p(pointer))
				*handle = copy_untagged_object(pointer);
#ifdef FACTOR_DEBUG
			assert((*handle)->h.hi_tag() == BIGNUM_TYPE);
#endif
		}
	}
}


/* Copy roots over at the start of GC, namely various constants, stacks,
the user environment and extra roots registered by local_roots.hpp */
void factorvm::copy_roots()
{
	copy_handle(&T);
	copy_handle(&bignum_zero);
	copy_handle(&bignum_pos_one);
	copy_handle(&bignum_neg_one);

	copy_registered_locals();
	copy_registered_bignums();

	if(!performing_compaction)
	{
		save_stacks();
		context *stacks = stack_chain;

		while(stacks)
		{
			copy_stack_elements(stacks->datastack_region,stacks->datastack);
			copy_stack_elements(stacks->retainstack_region,stacks->retainstack);

			copy_handle(&stacks->catchstack_save);
			copy_handle(&stacks->current_callback_save);

			mark_active_blocks(stacks);

			stacks = stacks->next;
		}
	}

	int i;
	for(i = 0; i < USER_ENV; i++)
		copy_handle(&userenv[i]);
}


cell factorvm::copy_next_from_nursery(cell scan)
{
	cell *obj = (cell *)scan;
	cell *end = (cell *)(scan + binary_payload_start((object *)scan));

	if(obj != end)
	{
		obj++;

		cell nursery_start = nursery.start;
		cell nursery_end = nursery.end;

		for(; obj < end; obj++)
		{
			cell pointer = *obj;

			if(!immediate_p(pointer))
			{
				check_data_pointer((object *)pointer);
				if(pointer >= nursery_start && pointer < nursery_end)
					*obj = copy_object(pointer);
			}
		}
	}

	return scan + untagged_object_size((object *)scan);
}


cell factorvm::copy_next_from_aging(cell scan)
{
	cell *obj = (cell *)scan;
	cell *end = (cell *)(scan + binary_payload_start((object *)scan));

	if(obj != end)
	{
		obj++;

		cell tenured_start = data->generations[data->tenured()].start;
		cell tenured_end = data->generations[data->tenured()].end;

		cell newspace_start = newspace->start;
		cell newspace_end = newspace->end;

		for(; obj < end; obj++)
		{
			cell pointer = *obj;

			if(!immediate_p(pointer))
			{
				check_data_pointer((object *)pointer);
				if(!(pointer >= newspace_start && pointer < newspace_end)
				   && !(pointer >= tenured_start && pointer < tenured_end))
					*obj = copy_object(pointer);
			}
		}
	}

	return scan + untagged_object_size((object *)scan);
}


cell factorvm::copy_next_from_tenured(cell scan)
{
	cell *obj = (cell *)scan;
	cell *end = (cell *)(scan + binary_payload_start((object *)scan));

	if(obj != end)
	{
		obj++;

		cell newspace_start = newspace->start;
		cell newspace_end = newspace->end;

		for(; obj < end; obj++)
		{
			cell pointer = *obj;

			if(!immediate_p(pointer))
			{
				check_data_pointer((object *)pointer);
				if(!(pointer >= newspace_start && pointer < newspace_end))
					*obj = copy_object(pointer);
			}
		}
	}

	mark_object_code_block((object *)scan);

	return scan + untagged_object_size((object *)scan);
}


void factorvm::copy_reachable_objects(cell scan, cell *end)
{
	if(collecting_gen == data->nursery())
	{
		while(scan < *end)
			scan = copy_next_from_nursery(scan);
	}
	else if(data->have_aging_p() && collecting_gen == data->aging())
	{
		while(scan < *end)
			scan = copy_next_from_aging(scan);
	}
	else if(collecting_gen == data->tenured())
	{
		while(scan < *end)
			scan = copy_next_from_tenured(scan);
	}
}


/* Prepare to start copying reachable objects into an unused zone */
void factorvm::begin_gc(cell requested_bytes)
{
	if(growing_data_heap)
	{
		if(collecting_gen != data->tenured())
			critical_error("Invalid parameters to begin_gc",0);

		old_data_heap = data;
		set_data_heap(grow_data_heap(old_data_heap,requested_bytes));
		newspace = &data->generations[data->tenured()];
	}
	else if(collecting_accumulation_gen_p())
	{
		/* when collecting one of these generations, rotate it
		with the semispace */
		zone z = data->generations[collecting_gen];
		data->generations[collecting_gen] = data->semispaces[collecting_gen];
		data->semispaces[collecting_gen] = z;
		reset_generation(collecting_gen);
		newspace = &data->generations[collecting_gen];
		clear_cards(collecting_gen,collecting_gen);
		clear_decks(collecting_gen,collecting_gen);
		clear_allot_markers(collecting_gen,collecting_gen);
	}
	else
	{
		/* when collecting a younger generation, we copy
		reachable objects to the next oldest generation,
		so we set the newspace so the next generation. */
		newspace = &data->generations[collecting_gen + 1];
	}
}


void factorvm::end_gc(cell gc_elapsed)
{
	gc_stats *s = &stats[collecting_gen];

	s->collections++;
	s->gc_time += gc_elapsed;
	if(s->max_gc_time < gc_elapsed)
		s->max_gc_time = gc_elapsed;

	if(growing_data_heap)
	{
		dealloc_data_heap(old_data_heap);
		old_data_heap = NULL;
		growing_data_heap = false;
	}

	if(collecting_accumulation_gen_p())
	{
		/* all younger generations except are now empty.
		if collecting_gen == data->nursery() here, we only have 1 generation;
		old-school Cheney collector */
		if(collecting_gen != data->nursery())
			reset_generations(data->nursery(),collecting_gen - 1);
	}
	else if(collecting_gen == data->nursery())
	{
		nursery.here = nursery.start;
	}
	else
	{
		/* all generations up to and including the one
		collected are now empty */
		reset_generations(data->nursery(),collecting_gen);
	}

	collecting_aging_again = false;
}


/* Collect gen and all younger generations.
If growing_data_heap_ is true, we must grow the data heap to such a size that
an allocation of requested_bytes won't fail */
void factorvm::garbage_collection(cell gen,bool growing_data_heap_,cell requested_bytes)
{
	if(gc_off)
	{
		critical_error("GC disabled",gen);
		return;
	}

	u64 start = current_micros();

	performing_gc = true;
	growing_data_heap = growing_data_heap_;
	collecting_gen = gen;

	/* we come back here if a generation is full */
	if(setjmp(gc_jmp))
	{
		/* We have no older generations we can try collecting, so we
		resort to growing the data heap */
		if(collecting_gen == data->tenured())
		{
			growing_data_heap = true;

			/* see the comment in unmark_marked() */
			unmark_marked(&code);
		}
		/* we try collecting aging space twice before going on to
		collect tenured */
		else if(data->have_aging_p()
			&& collecting_gen == data->aging()
			&& !collecting_aging_again)
		{
			collecting_aging_again = true;
		}
		/* Collect the next oldest generation */
		else
		{
			collecting_gen++;
		}
	}

	begin_gc(requested_bytes);

	/* initialize chase pointer */
	cell scan = newspace->here;

	/* collect objects referenced from stacks and environment */
	copy_roots();
	/* collect objects referenced from older generations */
	copy_cards();

	/* do some tracing */
	copy_reachable_objects(scan,&newspace->here);

	/* don't scan code heap unless it has pointers to this
	generation or younger */
	if(collecting_gen >= last_code_heap_scan)
	{
		code_heap_scans++;

		if(collecting_gen == data->tenured())
			free_unmarked(&code,(heap_iterator)factor::update_literal_and_word_references);
		else
			copy_code_heap_roots();

		if(collecting_accumulation_gen_p())
			last_code_heap_scan = collecting_gen;
		else
			last_code_heap_scan = collecting_gen + 1;
	}

	cell gc_elapsed = (current_micros() - start);

	end_gc(gc_elapsed);

	performing_gc = false;
}


void factorvm::gc()
{
	garbage_collection(data->tenured(),false,0);
}


inline void factorvm::vmprim_gc()
{
	gc();
}

PRIMITIVE(gc)
{
	PRIMITIVE_GETVM()->vmprim_gc();
}

inline void factorvm::vmprim_gc_stats()
{
	growable_array result(this);

	cell i;
	u64 total_gc_time = 0;

	for(i = 0; i < max_gen_count; i++)
	{
		gc_stats *s = &stats[i];
		result.add(allot_cell(s->collections));
		result.add(tag<bignum>(long_long_to_bignum(s->gc_time)));
		result.add(tag<bignum>(long_long_to_bignum(s->max_gc_time)));
		result.add(allot_cell(s->collections == 0 ? 0 : s->gc_time / s->collections));
		result.add(allot_cell(s->object_count));
		result.add(tag<bignum>(long_long_to_bignum(s->bytes_copied)));

		total_gc_time += s->gc_time;
	}

	result.add(tag<bignum>(ulong_long_to_bignum(total_gc_time)));
	result.add(tag<bignum>(ulong_long_to_bignum(cards_scanned)));
	result.add(tag<bignum>(ulong_long_to_bignum(decks_scanned)));
	result.add(tag<bignum>(ulong_long_to_bignum(card_scan_time)));
	result.add(allot_cell(code_heap_scans));

	result.trim();
	dpush(result.elements.value());
}

PRIMITIVE(gc_stats)
{
	PRIMITIVE_GETVM()->vmprim_gc_stats();
}

void factorvm::clear_gc_stats()
{
	for(cell i = 0; i < max_gen_count; i++)
		memset(&stats[i],0,sizeof(gc_stats));

	cards_scanned = 0;
	decks_scanned = 0;
	card_scan_time = 0;
	code_heap_scans = 0;
}

inline void factorvm::vmprim_clear_gc_stats()
{
	clear_gc_stats();
}

PRIMITIVE(clear_gc_stats)
{
	PRIMITIVE_GETVM()->vmprim_clear_gc_stats();
}

/* classes.tuple uses this to reshape tuples; tools.deploy.shaker uses this
   to coalesce equal but distinct quotations and wrappers. */
inline void factorvm::vmprim_become()
{
	array *new_objects = untag_check<array>(dpop());
	array *old_objects = untag_check<array>(dpop());

	cell capacity = array_capacity(new_objects);
	if(capacity != array_capacity(old_objects))
		critical_error("bad parameters to become",0);

	cell i;

	for(i = 0; i < capacity; i++)
	{
		tagged<object> old_obj(array_nth(old_objects,i));
		tagged<object> new_obj(array_nth(new_objects,i));

		if(old_obj != new_obj)
			old_obj->h.forward_to(new_obj.untagged());
	}

	gc();

	/* If a word's definition quotation was in old_objects and the
	   quotation in new_objects is not compiled, we might leak memory
	   by referencing the old quotation unless we recompile all
	   unoptimized words. */
	compile_all_words();
}

PRIMITIVE(become)
{
	PRIMITIVE_GETVM()->vmprim_become();
}

void factorvm::inline_gc(cell *gc_roots_base, cell gc_roots_size)
{
	for(cell i = 0; i < gc_roots_size; i++)
		gc_locals.push_back((cell)&gc_roots_base[i]);

	garbage_collection(data->nursery(),false,0);

	for(cell i = 0; i < gc_roots_size; i++)
		gc_locals.pop_back();
}

VM_ASM_API void inline_gc(cell *gc_roots_base, cell gc_roots_size, factorvm *myvm)
{
	ASSERTVM();
	VM_PTR->inline_gc(gc_roots_base,gc_roots_size);
}

}
