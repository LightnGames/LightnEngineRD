@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\LevelConverter.py" %%f
)
pause