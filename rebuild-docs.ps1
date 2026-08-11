$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

mkdocs build --config-file ../mkdocs.yml
