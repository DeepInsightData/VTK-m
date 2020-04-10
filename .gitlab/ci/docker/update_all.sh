#!/bin/sh

set -e
set -x

# data is expected to be a string of the form YYYYMMDD
readonly date="$1"

cd centos7/cuda10.2
sudo docker build -t kitware/vtkm:ci-centos7_cuda10.2-$date .
cd ../..

cd rhel8/cuda10.2
sudo docker build -t kitware/vtkm:ci-rhel8_cuda10.2-$date .
cd ../..

cd ubuntu1604/base
sudo docker build -t kitware/vtkm:ci-ubuntu1604-$date .
cd ../..

cd ubuntu1604/cuda9.2
sudo docker build -t kitware/vtkm:ci-ubuntu1604_cuda9.2-$date .
cd ../..

cd ubuntu1804/base
sudo docker build -t kitware/vtkm:ci-ubuntu1804-$date .
cd ../..

cd ubuntu1804/cuda10.1
sudo docker build -t kitware/vtkm:ci-ubuntu1804_cuda10.1-$date .
cd ../..

# sudo docker login --username=<docker_hub_name>
sudo docker push kitware/vtkm
sudo docker system prune