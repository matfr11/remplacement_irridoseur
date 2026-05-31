# Hook PreToolUse Bash : verifie que les docs sont a jour avant git push
$json = [Console]::In.ReadToEnd()
try { $j = $json | ConvertFrom-Json } catch { Write-Output '{}'; exit 0 }

if ($j.tool_input.command -notmatch 'git push') {
    Write-Output '{}'
    exit 0
}

$changed = git diff --name-only origin/main..HEAD 2>$null
if (-not $changed) { Write-Output '{}'; exit 0 }

$src  = $changed | Where-Object { $_ -match '^main/' }
$docs = $changed | Where-Object { $_ -match '^(docs|CHANGELOG|README)' }

if ($src -and -not $docs) {
    $msg = 'RAPPEL DOCUMENTATION : des fichiers source (main/) ont ete modifies dans les commits a pousser sans mise a jour de la documentation. Verifier et mettre a jour docs/claude/, docs/dev/, CHANGELOG.md et README.md si necessaire avant ce push.'
    $out = [ordered]@{
        hookSpecificOutput = [ordered]@{
            hookEventName   = 'PreToolUse'
            additionalContext = $msg
        }
    } | ConvertTo-Json -Compress
    Write-Output $out
} else {
    Write-Output '{}'
}
