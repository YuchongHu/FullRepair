Users can use `apt` to install the required libraries.

 - g++
 - make & cmake
 - nasm
 - libtool & autoconf
 - git

```bash
$  sudo apt update
$  sudo apt install g++ make cmake nasm libtool autoconf git
```

- **Intel®-storage-acceleration-library (ISA-L)**.

  ```bash
  $  git clone git://github.com/intel/isa-l.git
  $  cd isa-l
  $  ./autogen.sh
  $  ./configure; make; sudo make install
  ```

- **Sockpp**

  ```bash
  $  git clone git://github.com/fpagliughi/sockpp.git
  $  cd sockpp
  $  mkdir build ; cd build
  $  cmake ..
  $  make
  $  sudo make install
  $  sudo ldconfig
  ```

- **Wondershaper**

  ```bash
  $  git clone git://github.com/magnific0/wondershaper.git
  $  cd wondershaper
  $  sudo make install
  ```

- **SSH**

  ```bash
  $  sudo apt install openssh-client openssh-server
  $  ssh-keygen
  $  ssh-copy-id -i ~/.ssh/id_rsa.pub username@ip_address
  $  ssh username@ip_address
  ```
