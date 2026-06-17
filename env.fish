# From: https://stackoverflow.com/questions/19672673/can-a-fish-script-tell-what-directory-its-stored-in
set curdir (pwd)
set -x REACTOR_UC_PATH (cd (dirname (status -f)); and pwd)
cd $curdir

# Define the function
function do_lfc
    {$REACTOR_UC_PATH}/ulf/bin/ulfc-dev $argv
end

# Create an alias for the function
function lfcg
    do_lfc $argv
end
