@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\MaterialConverter.py" %%f
)
pause