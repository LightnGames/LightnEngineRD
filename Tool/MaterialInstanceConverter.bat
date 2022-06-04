@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\MaterialInstanceConverter.py" %%f
)
pause