# VHS demo for Muuk
# Generates an animated terminal GIF using charmbracelet/vhs

Output muuk_demo.gif

Require zsh
Require starship
Require muuk

Set Shell "zsh"
Set FontSize 16
# Set FontFamily "JetBrainsMono Nerd Font"
# Set LetterSpacing 1

Hide
Type "source ~/.zshrc && clear" Enter
Show

Type "muuk -v" Sleep 500ms Enter Sleep 1s

Type "muuk init" Sleep 500ms Enter Sleep 1s
Type "myproject" Sleep 500ms Enter Sleep 1s
Type "me" Sleep 500ms Enter Sleep 1s
Type "0.0.1" Sleep 500ms Enter Sleep 1s
Enter Sleep 500ms
Enter Sleep 500ms
Enter Sleep 500ms

Type "yes" Sleep 500ms Enter Sleep 1s

Type "muuk add p-ranav/argparse" Sleep 500ms Enter
Wait+Line /❯$/
Sleep 500ms

Type "muuk install" Sleep 500ms Enter
Wait+Line /❯$/
Sleep 500ms

Type "muuk build" Sleep 500ms Enter
Wait+Line /❯$/
Sleep 1s

Type "muuk run" Sleep 500ms Enter Sleep 3s
