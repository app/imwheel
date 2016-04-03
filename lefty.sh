#!/bin/sh

sed -e 's/_L/_R/g' < imwheelrc > imwheelrc.lefty
mv imwheelrc imwheelrc.righty
mv imwheelrc.lefty imwheelrc
