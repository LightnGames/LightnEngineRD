@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\TextureConverter.py" %%f
)
pause