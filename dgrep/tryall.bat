rem ******************
rem TESTS.S - must have s option
rem s f i
rem x 
rem x x
rem x   x
rem x x x
rem ******************
try -ts		<tests.s
if errorlevel 1 pause /\/\/\/\ Error: try -ts tests.s
rem ******************
try -tsf	<tests.s
if errorlevel 1 pause /\/\/\/\ Error: try -tsf tests.s
rem ******************
try -tsi	<tests.s
if errorlevel 1 pause /\/\/\/\ Error: try -tsi tests.s
rem ******************
try -tsfi	<tests.s
if errorlevel 1 pause /\/\/\/\ Error: try -tsfi tests.s
rem ******************
rem TESTS - s option not allowed
rem f i
rem 
rem x 
rem x x
rem   x
rem ******************
try -t 		<tests
if errorlevel 1 pause /\/\/\/\ Error: try -t tests
rem ******************
try -tf		<tests
if errorlevel 1 pause /\/\/\/\ Error: try -tf tests
rem ******************
try -tfi	<tests
if errorlevel 1 pause /\/\/\/\ Error: try -tfi tests
rem ******************
try -ti		<tests
if errorlevel 1 pause /\/\/\/\ Error: try -ti tests
rem ******************
rem TESTS.I - test i option
rem f i
rem x x
rem   x
rem ******************
try -tfi	<tests.i
if errorlevel 1 pause /\/\/\/\ Error: try -tfi tests.i
rem ******************
try -ti		<tests.i
if errorlevel 1 pause /\/\/\/\ Error: try -ti tests.i
rem ******************
rem Success!!!
