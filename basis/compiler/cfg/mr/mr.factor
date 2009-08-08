! Copyright (C) 2009 Slava Pestov.
! See http://factorcode.org/license.txt for BSD license.
USING: kernel namespaces accessors compiler.cfg compiler.cfg.registers
compiler.cfg.linearization compiler.cfg.gc-checks
compiler.cfg.linear-scan compiler.cfg.build-stack-frame ;
IN: compiler.cfg.mr

: build-mr ( cfg -- mr )
    dup cfg [
        cfg get reps>> representations set
        insert-gc-checks
        linear-scan
        flatten-cfg
        build-stack-frame
    ] with-variable ;