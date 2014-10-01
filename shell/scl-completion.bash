# main function bound to scl command
_scl()
{
  local cur prev opts
  COMPREPLY=()

  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  opts="-l --list"

  # handle options
  if [[ ${cur} == -* ]] ; then
    COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
    return 0
  fi

  local collections=($(find /etc/scl/prefixes -maxdepth 1 -mindepth 1 -type f -exec basename {} \; | sort -u))

  # handle scriptlets; the first parameter must be a scriptlet if it is not an option
  if ((COMP_CWORD == 1)); then
    # get array of scriptlets found throughout collections
    local scriptlets=()
    for col in ${collections[@]}; do
        local prefix=`cat /etc/scl/prefixes/$col`
        scriptlets+=($(find $prefix/$col/* -maxdepth 1 -type f -exec basename {} \; | sort -u))
    done
    scriptlets_str=`echo ${scriptlets[@]} | sed 's/ /\n/g'| sort -u`
    COMPREPLY=( $(compgen -W "$scriptlets_str register deregister" -- ${cur}) )
    return 0
  fi

  # handle commands; does not handle commands without single or double quotes
  if [[ ${cur} == \'* || ${cur} == \"* ]] ; then
    # it is a command do not reply with anything
    return 0
  fi

  # handle collections; if it is not an option or a command, it must be a collection
  if [ $prev == "register" ]; then
    compopt -o nospace
    COMPREPLY=( $(compgen -A directory ${cur}) )
    return 0
  fi
  COMPREPLY=( $(compgen -W "${collections[*]}" -- ${cur}) )
  return 0
}

# bind the scl command to the _scl function for completion
complete -F _scl scl
