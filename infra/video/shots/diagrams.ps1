$list = @(
 @{n='diag01-layers'; q='d=01-layers&seq=l1,l2,l3,l4&step=1800&delay=2500'; t=12},
 @{n='diag02-request'; q='d=02-request-flow&seq=renderer,browser,engine&step=2200&delay=2500'; t=13},
 @{n='diag03-rebuild'; q='d=03-rebuild-cost'; t=10},
 @{n='diag04-rebase'; q='d=04-rebase-cycle'; t=10})
foreach ($d in $list) { AppStart ('http://kurier.test/diagram.html?' + $d.q); Scene 'Okno'; MoveTo 1919 1079 100; Keys '{F5}'; RecStart; Start-Sleep $d.t; RecStop $d.n }
