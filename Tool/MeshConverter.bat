@echo off

for %%f in (%*) do (
  "%LTN_ROOT%\Tool\FBXConverter.exe" %%f
)
pause