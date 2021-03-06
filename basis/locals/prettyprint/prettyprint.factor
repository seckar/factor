! Copyright (C) 2007, 2008 Slava Pestov, Eduardo Cavazos.
! See http://factorcode.org/license.txt for BSD license.
USING: accessors assocs kernel locals locals.types
prettyprint.backend prettyprint.sections prettyprint.custom
sequences words ;
IN: locals.prettyprint

SYMBOL: |

: pprint-var ( var -- )
    #! Prettyprint a read/write local as its writer, just like
    #! in the input syntax: [| x! | ... x 3 + x! ]
    dup local-reader? [
        "local-writer" word-prop
    ] when pprint-word ;

: pprint-vars ( vars -- ) [ pprint-var ] each ;

M: lambda pprint*
    <flow
    \ [| pprint-word
    dup vars>> pprint-vars
    \ | pprint-word
    f <inset body>> pprint-elements block>
    \ ] pprint-word
    block> ;

: pprint-let ( let word -- )
    pprint-word
    [ body>> ] [ bindings>> ] bi
    \ | pprint-word
    t <inset
    <block
    [ <block [ pprint-var ] dip pprint* block> ] assoc-each
    block>
    \ | pprint-word
    <block pprint-elements block>
    block>
    \ ] pprint-word ;

M: let pprint* \ [let pprint-let ;

M: wlet pprint* \ [wlet pprint-let ;

M: let* pprint* \ [let* pprint-let ;

M: def pprint*
    <block \ :> pprint-word local>> pprint-word block> ;
