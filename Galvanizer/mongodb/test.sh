apt-get install -y wget
apt-get install -y gcc g++
apt-get install -y ncurses-dev
apt-get install -y global
apt-get install -y python

apt-get install -y mongodb-clients
apt-get install -y mongodb-server

PATH=$PATH:/usr/local/bin
export PATH

./listup.pl | grep gz | grep bind > gzlist

TESTFILE=gzlist
while read line; do
    cp $line .
    array=( `echo $line | tr -s '/' ' '`)
    last_index=`expr ${#array[@]} - 1`
    echo ${array[${last_index}]}
    d=`echo ${array[${last_index}]} | sed s/.tar.gz//g`
    echo $d

    tar zxvf ${array[${last_index}]}

    cp *py *pl *sh ./$d/
    cd $d

    currentdir=`pwd | awk -F'/' '{print $0}'`
    echo $currentdir

    array=( `echo $currentdir | tr -s '/' ' '`)
    last_index=`expr ${#array[@]} - 1`

    echo ${array[${last_index}]}
    currentdir=${array[${last_index}]}
    currentdir2=`echo $currentdir | sed -e "s/\-//g"`
    echo $currentdir2
    currentdir3=`echo $currentdir2 | sed -e "s/\///g"`
    echo $currentdir3
    currentdir4=`echo $currentdir3 | sed -e "s/\.//g"`
    echo $currentdir4

    chmod 755 *pl
    chmod 755 *py
    chmod 755 *sh
    gtags -v
    rm -rf list
    ./listup.pl | tee list
    rm -rf flist
    ./global-t.sh list | tee flist
    rm -rf flist2 
    ./1.pl flist | tee list-definition-$currentdir4
    rm -rf flist3 
    ./cut.pl list-definition-$currentdir4 | tee flist3
    rm -rf flist3-drem 
    ./drem.pl flist3 | tee flist3-drem
    rm -rf flist4
    python global-rx.py flist3-drem | tee flist4
    rm -rf flist5
    ./1.pl flist4 | tee list-callchain-$currentdir4

    cp list-definition-$currentdir4 ../
    cp list-callchain-$currentdir4 ../
    cd ..

    cd $d

    curl -kL  https://bootstrap.pypa.io/get-pip.py | python 
    pip install pymongo numpy

    currentdir=`pwd | awk -F'/' '{print $0}'`
    echo $currentdir

    array=( `echo $currentdir | tr -s '/' ' '`)
    last_index=`expr ${#array[@]} - 1`

    echo ${array[${last_index}]}
    currentdir=${array[${last_index}]}
    currentdir2=`echo $currentdir | sed -e "s/\-//g"`
    echo $currentdir2
    currentdir3=`echo $currentdir2 | sed -e "s/\///g"`
    echo $currentdir3

    currentdir4=`echo $currentdir3 | sed -e "s/\.//g"`
    echo $currentdir4

    python flist2.py list-definition-$currentdir4 $currentdir4
    python flist5.py list-callchain-$currentdir4 $currentdir4
    python ex3.py $currentdir4

    cd ..
    
done < $TESTFILE
