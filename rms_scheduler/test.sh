make clean
make
sudo insmod mp2.ko
./userapp 200 1000 20 &
./userapp 1000 10 20 &
./userapp 5000 10 20 &
./userapp 500 10 20 &
cat /proc/mp2/status