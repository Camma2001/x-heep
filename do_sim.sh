make questasim-sim FUSESOC_PARAM="--X_EXT=1"
cd ./build/openhwgroup.org_systems_core-v-mini-mcu_0/sim-modelsim/
make run-gui PLUSARGS="c firmware=../../../sw/build/main.hex" 
cd ../../..