USING: ui.gadgets ui.gadgets.scrollers namespaces tools.test
kernel models models.compose models.range ui.gadgets.viewports
ui.gadgets.labels ui.gadgets.grids ui.gadgets.frames
ui.gadgets.sliders math math.vectors arrays sequences
tools.test.ui math.geometry.rect accessors ;
IN: ui.gadgets.scrollers.tests

[ ] [
    <gadget> "g" set
    "g" get <scroller> "s" set
] unit-test

[ { 100 200 } ] [
    { 100 200 } "g" get scroll>rect
    "s" get follows>> rect-loc
] unit-test

[ ] [ "s" get scroll>bottom ] unit-test
[ t ] [ "s" get follows>> ] unit-test

[ ] [
    <gadget> dup "g" set
    10 1 0 100 <range> 20 1 0 100 <range> 2array <compose>
    <viewport> "v" set
] unit-test

"v" get [
    [ { 10 20 } ] [ "v" get model>> range-value ] unit-test

    [ { 10 20 } ] [ "g" get rect-loc vneg { 3 3 } v+ ] unit-test
] with-grafted-gadget

[ ] [
    <gadget> { 100 100 } >>dim
    dup "g" set <scroller> "s" set
] unit-test

[ ] [ "s" get { 50 50 } >>dim drop ] unit-test

[ ] [ "s" get layout ] unit-test

"s" get [
    [ { 34 34 } ] [ "s" get viewport>> rect-dim ] unit-test

    [ { 106 106 } ] [ "s" get viewport>> viewport-dim ] unit-test

    [ ] [ { 0 0 } "s" get scroll ] unit-test

    [ { 0 0 } ] [ "s" get model>> range-min-value ] unit-test

    [ { 106 106 } ] [ "s" get model>> range-max-value ] unit-test

    [ ] [ { 10 20 } "s" get scroll ] unit-test

    [ { 10 20 } ] [ "s" get model>> range-value ] unit-test

    [ { 10 20 } ] [ "s" get viewport>> model>> range-value ] unit-test

    [ { 10 20 } ] [ "g" get rect-loc vneg { 3 3 } v+ ] unit-test
] with-grafted-gadget

<gadget> { 600 400 } >>dim "g1" set
<gadget> { 600 10 } >>dim "g2" set
"g2" get "g1" get swap add-gadget drop

"g1" get <scroller>
{ 300 300 } >>dim
dup layout
"s" set

[ t ] [
    10 [
        drop
        "g2" get scroll>gadget
        "s" get layout
        "s" get scroller-value
    ] map [ { 3 0 } = ] all?
] unit-test

[ ] [ "Hi" <label> dup "l" set <scroller> "s" set ] unit-test

[ t ] [ "l" get find-scroller "s" get eq? ] unit-test
[ t ] [ "l" get dup find-scroller viewport>> swap child? ] unit-test
[ t ] [ "l" get find-scroller* "s" get eq? ] unit-test
[ f ] [ "s" get viewport>> find-scroller* ] unit-test
[ t ] [ "s" get @right grid-child slider? ] unit-test
[ f ] [ "s" get @right grid-child find-scroller* ] unit-test

\ <scroller> must-infer