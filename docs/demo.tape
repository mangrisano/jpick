# jpick — terminal demo (VHS tape)
# Regenerate with:  vhs docs/demo.tape   (run from the repo root)
# Requires: vhs (brew install vhs) and jpick on PATH (brew install mangrisano/jpick/jpick).

Output docs/demo.gif

Set Shell "zsh"
Set FontSize 16
Set Width 1000
Set Height 660
Set Padding 24
Set Theme "Dracula"
Set TypingSpeed 55ms

Hide
Type "cd /tmp"
Enter
Type `printf '%s' '{"repo":"jpick","langs":["c++","cmake"],"authors":[{"name":"Michele","commits":120},{"name":"Ada","commits":8}]}' > sample.json`
Enter
Type "clear"
Enter
Show

Sleep 500ms
Type "jpick -p '.' sample.json"
Sleep 800ms
Enter
Sleep 3000ms

Hide
Type "clear"
Enter
Show

Sleep 400ms
Type "jpick '.repo' sample.json"
Sleep 700ms
Enter
Sleep 1800ms

Type "jpick '.authors[0].name' sample.json"
Sleep 700ms
Enter
Sleep 1800ms

Type "jpick '.langs[]' sample.json"
Sleep 700ms
Enter
Sleep 1800ms

Type `jpick -r '.authors[] | "\(.name) has \(.commits) commits"' sample.json`
Sleep 800ms
Enter
Sleep 3000ms
