@echo off

set "SourceDir=%~1/shaders"
set "TargetDir=%~2/shaders"

xcopy /E /I /Y /D "%SourceDir%" "%TargetDir%"
