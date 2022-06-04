@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\StaticMeshConverter.py" %%f
)
pause