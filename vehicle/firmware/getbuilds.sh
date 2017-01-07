#!/bin/bash

rm *.hex

for dir in ../OVMS.X/dist/* ; do
  cp -a $dir/production/OVMS.X.production.hex $(basename $dir).hex
done

ls -l *.hex
