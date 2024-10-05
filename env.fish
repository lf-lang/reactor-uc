# From: https://stackoverflow.com/questions/19672673/can-a-fish-script-tell-what-directory-its-stored-in
set -x REACTOR_UC_PATH (cd (dirname (status -f)); and pwd)

# Define the function
function do_lfc
    {$REACTOR_UC_PATH}/lfc/bin/lfc-dev $argv
end

# Create an alias for the function
function lfcg
    do_lfc $argv
end
