# lib_i2c
i2c control lib
```
Usage: ./lib_i2c [-D:device] [-b] [-w]

  -D --Device         Control Device node
  -b --byte_read      byte_read func used
  -w --word_read      word_read func used

  e.g) find i2c device from i2c-node
       lib_i2c -D /dev/i2c-0
```
