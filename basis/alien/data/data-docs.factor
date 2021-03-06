USING: alien alien.c-types help.syntax help.markup libc kernel.private
byte-arrays math strings hashtables alien.syntax alien.strings sequences
io.encodings.string debugger destructors vocabs.loader ;
IN: alien.data

HELP: <c-array>
{ $values { "len" "a non-negative integer" } { "c-type" "a C type" } { "array" byte-array } }
{ $description "Creates a byte array large enough to hold " { $snippet "n" } " values of a C type." }
{ $notes "The appropriate specialized array vocabulary must be loaded; otherwise, an error will be thrown. The vocabulary can be loaded with the " { $link require-c-array } " word. See the " { $vocab-link "specialized-arrays" } " vocabulary for details on the underlying sequence type constructed." }
{ $errors "Throws an error if the type does not exist, the necessary specialized array vocabulary is not loaded, or the requested size is negative." } ;

HELP: <c-object>
{ $values { "type" "a C type" } { "array" byte-array } }
{ $description "Creates a byte array suitable for holding a value with the given C type." }
{ $errors "Throws an " { $link no-c-type } " error if the type does not exist." } ;

{ <c-object> malloc-object } related-words

HELP: memory>byte-array
{ $values { "alien" c-ptr } { "len" "a non-negative integer" } { "byte-array" byte-array } }
{ $description "Reads " { $snippet "len" } " bytes starting from " { $snippet "base" } " and stores them in a new byte array." } ;

HELP: byte-array>memory
{ $values { "byte-array" byte-array } { "base" c-ptr } }
{ $description "Writes a byte array to memory starting from the " { $snippet "base" } " address." }
{ $warning "This word is unsafe. Improper use can corrupt memory." } ;

HELP: malloc-array
{ $values { "n" "a non-negative integer" } { "type" "a C type" } { "alien" alien } }
{ $description "Allocates an unmanaged memory block large enough to hold " { $snippet "n" } " values of a C type, then wraps the memory in a sequence object using " { $link <c-direct-array> } "." }
{ $notes "The appropriate specialized array vocabulary must be loaded; otherwise, an error will be thrown. The vocabulary can be loaded with the " { $link require-c-array } " word. See the " { $vocab-link "specialized-arrays" } " vocabulary for details on the underlying sequence type constructed." }
{ $warning "Don't forget to deallocate the memory with a call to " { $link free } "." }
{ $errors "Throws an error if the type does not exist, if the requested size is negative, if a direct specialized array class appropriate to the type is not loaded, or if memory allocation fails." } ;

HELP: malloc-object
{ $values { "type" "a C type" } { "alien" alien } }
{ $description "Allocates an unmanaged memory block large enough to hold a value of a C type." }
{ $warning "Don't forget to deallocate the memory with a call to " { $link free } "." }
{ $errors "Throws an error if the type does not exist or if memory allocation fails." } ;

HELP: malloc-byte-array
{ $values { "byte-array" byte-array } { "alien" alien } }
{ $description "Allocates an unmanaged memory block of the same size as the byte array, and copies the contents of the byte array there." }
{ $warning "Don't forget to deallocate the memory with a call to " { $link free } "." }
{ $errors "Throws an error if memory allocation fails." } ;

{ <c-array> <c-direct-array> malloc-array } related-words

{ string>alien alien>string malloc-string } related-words

ARTICLE: "malloc" "Manual memory management"
"Sometimes data passed to C functions must be allocated at a fixed address. See " { $link "byte-arrays-gc" } " for an explanation of when this is the case."
$nl
"Allocating a C datum with a fixed address:"
{ $subsection malloc-object }
{ $subsection malloc-array }
{ $subsection malloc-byte-array }
"There is a set of words in the " { $vocab-link "libc" } " vocabulary which directly call C standard library memory management functions:"
{ $subsection malloc }
{ $subsection calloc }
{ $subsection realloc }
"You must always free pointers returned by any of the above words when the block of memory is no longer in use:"
{ $subsection free }
"Utilities for automatically freeing memory in conjunction with " { $link with-destructors } ":"
{ $subsection &free }
{ $subsection |free }
"The " { $link &free } " and " { $link |free } " words are generated using " { $link "alien.destructors" } "."
$nl
"You can unsafely copy a range of bytes from one memory location to another:"
{ $subsection memcpy }
"You can copy a range of bytes from memory into a byte array:"
{ $subsection memory>byte-array }
"You can copy a byte array to memory unsafely:"
{ $subsection byte-array>memory } ;


ARTICLE: "c-byte-arrays" "Passing data in byte arrays"
"Instances of the " { $link byte-array } " class can be passed to C functions; the C function receives a pointer to the first element of the array."
$nl
"Byte arrays can be allocated directly with a byte count using the " { $link <byte-array> } " word. However in most cases, instead of computing a size in bytes directly, it is easier to use a higher-level word which expects C type and outputs a byte array large enough to hold that type:"
{ $subsection <c-object> }
{ $subsection <c-array> }
{ $warning
"The Factor garbage collector can move byte arrays around, and code passing byte arrays to C must obey important guidelines. See " { $link "byte-arrays-gc" } "." }
{ $see-also "c-arrays" } ;

ARTICLE: "c-data" "Passing data between Factor and C"
"Two defining characteristics of Factor are dynamic typing and automatic memory management, which are somewhat incompatible with the machine-level data model exposed by C. Factor's C library interface defines its own set of C data types, distinct from Factor language types, together with automatic conversion between Factor values and C types. For example, C integer types must be declared and are fixed-width, whereas Factor supports arbitrary-precision integers."
$nl
"Furthermore, Factor's garbage collector can move objects in memory; for a discussion of the consequences, see " { $link "byte-arrays-gc" } "."
{ $subsection "c-types-specs" }
{ $subsection "c-byte-arrays" }
{ $subsection "malloc" }
{ $subsection "c-strings" }
{ $subsection "c-arrays" }
{ $subsection "c-out-params" }
"Important guidelines for passing data in byte arrays:"
{ $subsection "byte-arrays-gc" }
"C-style enumerated types are supported:"
{ $subsection POSTPONE: C-ENUM: }
"C types can be aliased for convenience and consitency with native library documentation:"
{ $subsection POSTPONE: TYPEDEF: }
"New C types can be defined:"
{ $subsection "c-structs" }
{ $subsection "c-unions" }
"A utility for defining " { $link "destructors" } " for deallocating memory:"
{ $subsection "alien.destructors" }
{ $see-also "aliens" } ;
HELP: malloc-string
{ $values { "string" string } { "encoding" "an encoding descriptor" } { "alien" c-ptr } }
{ $description "Encodes a string together with a trailing null code point using the given encoding, and stores the resulting bytes in a freshly-allocated unmanaged memory block." }
{ $warning "Don't forget to deallocate the memory with a call to " { $link free } "." }
{ $errors "Throws an error if one of the following conditions occurs:"
    { $list
        "the string contains null code points"
        "the string contains characters not representable using the encoding specified"
        "memory allocation fails"
    }
} ;

HELP: require-c-array
{ $values { "c-type" "a C type" } }
{ $description "Generates a specialized array of " { $snippet "c-type" } " using the " { $link <c-array> } " or " { $link <c-direct-array> } " vocabularies." }
{ $notes "This word must be called inside a compilation unit. See the " { $vocab-link "specialized-arrays" } " vocabulary for details on the underlying sequence types loaded." } ;

HELP: <c-direct-array>
{ $values { "alien" c-ptr } { "len" integer } { "c-type" "a C type" } { "array" "a specialized direct array" } }
{ $description "Constructs a new specialized array of length " { $snippet "len" } " and element type " { $snippet "c-type" } " over the range of memory referenced by " { $snippet "alien" } "." }
{ $notes "The appropriate specialized array vocabulary must be loaded; otherwise, an error will be thrown. The vocabulary can be loaded with the " { $link require-c-array } " word. See the " { $vocab-link "specialized-arrays" } " vocabulary for details on the underlying sequence type constructed." } ;

ARTICLE: "c-strings" "C strings"
"C string types are arrays with shape " { $snippet "{ \"char*\" encoding }" } ", where " { $snippet "encoding" } " is an encoding descriptor. The type " { $snippet "\"char*\"" } " is an alias for " { $snippet "{ \"char*\" utf8 }" } ". See " { $link "encodings-descriptors" } " for information about encoding descriptors."
$nl
"Passing a Factor string to a C function expecting a C string allocates a " { $link byte-array } " in the Factor heap; the string is then converted to the requested format and a raw pointer is passed to the function."
$nl
"If the conversion fails, for example if the string contains null bytes or characters with values higher than 255, a " { $link c-string-error. } " is thrown."
$nl
"Care must be taken if the C function expects a " { $snippet "char*" } " with a length in bytes, rather than a null-terminated " { $snippet "char*" } "; passing the result of calling " { $link length } " on the string object will not suffice. This is because a Factor string of " { $emphasis "n" } " characters will not necessarily encode to " { $emphasis "n" } " bytes. The correct idiom for C functions which take a string with a length is to first encode the string using " { $link encode } ", and then pass the resulting byte array together with the length of this byte array."
$nl
"Sometimes a C function has a parameter type of " { $snippet "void*" } ", and various data types, among them strings, can be passed in. In this case, strings are not automatically converted to aliens, and instead you must call one of these words:"
{ $subsection string>alien }
{ $subsection malloc-string }
"The first allocates " { $link byte-array } "s, and the latter allocates manually-managed memory which is not moved by the garbage collector and has to be explicitly freed by calling " { $link free } ". See " { $link "byte-arrays-gc" } " for a discussion of the two approaches."
$nl
"A word to read strings from arbitrary addresses:"
{ $subsection alien>string }
"For example, if a C function returns a " { $snippet "char*" } " but stipulates that the caller must deallocate the memory afterward, you must define the function as returning " { $snippet "void*" } ", and call one of the above words before passing the pointer to " { $link free } "." ;

