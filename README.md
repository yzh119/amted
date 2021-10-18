# cse550 assignment 1 part b

A toy AMTED server (proposed in the [Flash paper](https://www.usenix.org/legacy/events/usenix99/full_papers/pai/pai.pdf) in ATC 99) implementation.

## Usage

### Compile

Compile from source code
```
mkdir build; cd build
cmake .. && make -j
sudo make install
```

Note that for Mac only the client would be compiled, because the server requires epoll, which is not supported on Mac.

### Run

Run the following command to launch server:
```
550amtedserver 127.0.0.1 PORT
```

where `PORT` must be a valid integer.

Run the following command to launch client:
```
550amtedclient SERVER_IP PORT
```

where `SERVER_IP` refers to the server ipv4 address, and `OPEN` must be equal to the port server uses.

the client hangs until you type in a path file ended with an enter.

### Test

There are some python scripts under `tests/` folder.

We can generate a list of random files on the server (e.g. `99.99.99.99`), then launch a server rooted at `tests/data`:

```
cd tests
python3 gen_data.py
cd data
550amtedserver 127.0.0.1 23333
```

and we can test download files on other instance(s):

```
cd tests
python3 gen_file_list.py | 550amtedclient 99.99.99.99 23333
```
