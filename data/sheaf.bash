# bash completion for sheaf
_sheaf()
{
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local commands="list search open favorite tag forget clear-recent clean groups paths import-thingy export import help version"
    if [ "$COMP_CWORD" -eq 1 ]; then
        COMPREPLY=($(compgen -W "$commands --quick --help --version" -- "$cur"))
        return
    fi
    case "${COMP_WORDS[1]}" in
        favorite|tag) [ "$COMP_CWORD" -eq 2 ] && COMPREPLY=($(compgen -W "add remove list" -- "$cur")) ;;
        clean) COMPREPLY=($(compgen -W "--dry-run" -- "$cur")) ;;
        open|forget|export|import) COMPREPLY=($(compgen -f -- "$cur")) ;;
    esac
}
complete -F _sheaf sheaf
