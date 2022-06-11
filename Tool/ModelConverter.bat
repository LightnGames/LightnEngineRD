@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\ModelConverter.py" %%f
)
pause