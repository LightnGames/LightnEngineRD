@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\MeshConverter.py" %%f
)
pause