@echo off

for %%f in (%*) do (
python %LTN_ROOT%"\Tool\PipelineSetConverter.py" %%f
)
pause