@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\ShaderBuilder.py" %%f
)
pause