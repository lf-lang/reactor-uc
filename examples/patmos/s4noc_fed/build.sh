
LF_MAIN=s4noc_fed
BIN_DIR=bin
CC=patmos-clang
if jtagconfig 2>/dev/null | grep -q "USB-Blaster"; then
    DEF_TOOL=f
    echo "USB-Blaster detected."
else
    DEF_TOOL=e
    echo "USB-Blaster not detected."
fi

usage() {
  echo "Usage: $0 [-e] [-f] [-h]"
  echo "  -e    Set default action to emulate"
  echo "  -f    Set default action to FPGA"
  echo "  -h    Show this help message"
}

while getopts ":fedh" opt; do 
  case $opt in
    f) DEF_TOOL=f;;
    e) DEF_TOOL=e;;
    h) usage; exit 0;;
    d) rm -f $REACTOR_UC_PATH/src/*.bc;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1;;
    \?) echo "Invalid option -$OPTARG" >&2; exit 1;;
  esac
done

make clean -C ./sender
make clean -C ./receiver
rm -f $REACTOR_UC_PATH/src/scheduler.bc $REACTOR_UC_PATH/src/platform.bc $REACTOR_UC_PATH/src/network_channel.bc $REACTOR_UC_PATH/src/environment.bc
make clean 

make all -C ./sender
make all -C ./receiver
echo "Linking sender and receiver to create executable"
make all || exit 1

read -n 1 -t 5 -p "Choose action: [e]mulate or [f]pga? (default: $DEF_TOOL) " action
action=${action:-$DEF_TOOL}
if [[ "$action" == "e" ]]; then
    patemu $BIN_DIR/$LF_MAIN.elf
elif [[ "$action" == "f" ]]; then
    rm -f ~/t-crest/patmos/tmp/$LF_MAIN.elf
    mv $BIN_DIR/$LF_MAIN.elf ~/t-crest/patmos/tmp/$LF_MAIN.elf
    RETRIES=5
    DELAY=10
    attempt=0

    while ! jtagconfig | grep -q "USB-Blaster"; do
        attempt=$((attempt+1))
        if [ "$attempt" -ge "$RETRIES" ]; then
            echo "USB-Blaster not detected after $RETRIES attempts."
            break
        fi
        echo "USB-Blaster not detected. Retry $attempt/$RETRIES in ${DELAY}s..."
        sleep "$DELAY"
    done
    make -C ~/t-crest/patmos APP=$LF_MAIN config download
else
    echo "Invalid option. Please choose 'e' for emulate or 'f' for fpga."
fi
