#!/bin/bash
# ##########################################################
# ALPS(Android4.1 based) build environment profile setting
# ##########################################################
# Overwrite JAVA_HOME environment variable setting if already exists
JAVA_HOME=/usr/lib/jvm/java-6-sun-1.6.0.26
export JAVA_HOME

# Overwrite ANDROID_JAVA_HOME environment variable setting if already exists
ANDROID_JAVA_HOME=$JAVA_HOME
export ANDROID_JAVA_HOME

# Overwrite PATH environment setting for JDK & arm-eabi if already exists
PATH=$JAVA_HOME/bin:$PWD/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.6/bin:$PATH
export PATH

# Add MediaTek developed Python libraries path into PYTHONPATH
if [ -z "$PYTHONPATH" ]; then
  PYTHONPATH=$PWD/mediatek/build/tools
else
  PYTHONPATH=$PWD/mediatek/build/tools:$PYTHONPATH
fi
export PYTHONPATH

# MTK customized functions
# Adatping MTK build system with custmer(L)'s setting
function m()
{
    local HERE=$PWD
	local HW_REV=""
    T=$(gettop)

	if [ "${1:0:3}" = "rev" ]; then
		HW_REV="-$1"
		echo "HW_REVISION : $HW_REV"
		shift
	elif [ "${1:0:4}" = "-rev" ]; then
		HW_REV=$1
		echo "HW_REVISION : $HW_REV"
		shift
	fi
	
    if [ "$T" ]; then
        local LOG_FILE=$T/build_log.txt
        cd $T
        time ./makeMtk $HW_REV -t -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE,$@ \
            $TARGET_PRODUCT new | tee -a $LOG_FILE
        if [ ${PIPESTATUS[0]} -ne 0 ]; then
            cd $HERE > /dev/null
            echo -e "\nBuild log was written to '$LOG_FILE'."
            echo "Error: Building failed"
            return 1
        fi
        cd $HERE > /dev/null
        echo -e "\nBuild log was written to '$LOG_FILE'."
    else
        echo "Couldn't locate the top of the tree.  Try setting TOP."
    fi
}

function mr()
{
    local HERE=$PWD
    T=$(gettop)
    if [ "$T" ]; then
        local LOG_FILE=$T/build_log.txt
        cd $T
        time ./makeMtk -t -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE,$@ \
            $TARGET_PRODUCT r dr| tee -a $LOG_FILE
        if [ ${PIPESTATUS[0]} -ne 0 ]; then
            cd $HERE > /dev/null
            echo -e "\nBuild log was written to '$LOG_FILE'."
            echo "Error: Building failed"
            return 1
        fi
        cd $HERE > /dev/null
        echo -e "\nBuild log was written to '$LOG_FILE'."
    else
        echo "Couldn't locate the top of the tree.  Try setting TOP."
    fi
}

function mm()
{
    local HERE=$PWD
    # If we're sitting in the root of the build tree, just do a
    # normal make.
    if [ -f build/core/envsetup.mk -a -f Makefile ]; then
        ./makeMtk -t -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE,$@ \
            $TARGET_PRODUCT new
    else
        # Find the closest Android.mk file.
        T=$(gettop)
        local M=$(findmakefile)
        # Remove the path to top as the makefilepath needs to be relative
        local M=`echo $M|sed 's:'$T'/::'`
        local P=`echo $M|sed 's:'/Android.mk'::'`
        if [ ! "$T" ]; then
            echo "Couldn't locate the top of the tree. Try setting TOP."
        elif [ ! "$M" ]; then
            echo "Couldn't locate a makefile from the current directory."
        else
            (cd $T;./makeMtk -t -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE \
                $TARGET_PRODUCT mm $P)
            cd $HERE > /dev/null
        fi
    fi
}

function mmm()
{
    T=$(gettop)
    if [ "$T" ]; then
        local MAKEFILE=
        local ARGS=
        local DIR TO_CHOP
        local DASH_ARGS=$(echo "$@" | awk -v RS=" " -v ORS=" " '/^-.*$/')
        local DIRS=$(echo "$@" | awk -v RS=" " -v ORS=" " '/^[^-].*$/')
        local SNOD=
        for DIR in $DIRS ; do
            DIR=`echo $DIR | sed -e 's:/$::'`
            if [ -f $DIR/Android.mk ]; then
                TO_CHOP=`(cd -P -- $T && pwd -P) | wc -c | tr -d ' '`
                TO_CHOP=`expr $TO_CHOP + 1`
                START=`PWD= /bin/pwd`
                MFILE=`echo $START | cut -c${TO_CHOP}-`
                if [ "$MFILE" = "" ] ; then
                    MFILE=$DIR/Android.mk
                else
                    MFILE=$MFILE/$DIR/Android.mk
                fi
                MAKEFILE="$MAKEFILE $MFILE"
            else
                if [ "$DIR" = snod ]; then
                    ARGS="$ARGS snod"
                    SNOD=snod
                elif [ "$DIR" = showcommands ]; then
                    ARGS="$ARGS showcommands"
                elif [ "$DIR" = dist ]; then
                    ARGS="$ARGS dist"
                elif [ "$DIR" = incrementaljavac ]; then
                    ARGS="$ARGS incrementaljavac"
                else
                    echo "No Android.mk in $DIR."
                    return 1
                fi
            fi
            (cd $T; ./makeMtk -t -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE,SNOD=$SNOD \
                $TARGET_PRODUCT mm $DIR)           
        done
    else
        echo "Couldn't locate the top of the tree.  Try setting TOP."
    fi
}

function create_link
{
    MAKEFILE_PATH=`/bin/ls device/*/*/$1.mk 2>/dev/null`

    TARGET_PRODUCT=`cat $MAKEFILE_PATH | grep PRODUCT_NAME | awk '{print $3}'`
    BASE_PROJECT=`cat $MAKEFILE_PATH | grep BASE_PROJECT | awk '{print $3}'`

    if [ ! "x$BASE_PROJECT" == x ]; then
        echo "Create symbolic link - $TARGET_PRODUCT based on $BASE_PROJECT"
        if [ ! -e "./mediatek/config/$TARGET_PRODUCT" ]; then
            ln -s `pwd`/mediatek/config/$BASE_PROJECT ./mediatek/config/$TARGET_PRODUCT
        fi
        if [ ! -e "./mediatek/custom/$TARGET_PRODUCT" ]; then
            ln -s `pwd`/mediatek/custom/$BASE_PROJECT ./mediatek/custom/$TARGET_PRODUCT
        fi
    else
        echo ./mediatek/config/$TARGET_PRODUCT
        if [ ! -e "./mediatek/config/$TARGET_PRODUCT" ]; then
            echo "NO BASE_PROJECT!!"
            return 1
        fi
    fi

	./mediatek/build/tools/copyMtkVendor.pl $TARGET_PRODUCT $BASE_PROJECT	
}

function snod()
{
    T=$(gettop)
    if [ "$T" ]; then
        ./makeMtk -t -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE,$@ \
            $TARGET_PRODUCT snod
    else
        echo "Couldn't locate the top of the tree.  Try setting TOP."
    fi
}

function mtk_custgen()
{
    T=$(gettop)
    if [ "$T" ]; then
        rm -rf mediatek/config/out/$TARGET_PRODUCT
        ./makeMtk -o=TARGET_BUILD_VARIANT=$TARGET_BUILD_VARIANT,TARGET_BUILD_TYPE=$TARGET_BUILD_TYPE $TARGET_PRODUCT custgen
    else
        echo "Couldn't locate the top of the tree.  Try setting TOP."
    fi
}

function cleanv()
{
	 repo forall -c git clean -xdf; repo forall -c git reset --hard;
}

function zipsymbol()
{
    croot

    if [ -e "$ANDROID_PRODUCT_OUT/symbols/vmlinux" ]; then
        rm "$ANDROID_PRODUCT_OUT/symbols/vmlinux"
    fi
    cp ./kernel/out/vmlinux $ANDROID_PRODUCT_OUT/symbols/

    if [ -e "$ANDROID_PRODUCT_OUT/symbols/BPLGUInfoCustomApp_MT6575_*" ]; then
        rm "$ANDROID_PRODUCT_OUT/symbols/BPLGUInfoCustomApp_MT6575_*"
    fi
    cp ./mediatek/custom/common/modem/${TARGET_PRODUCT}_hspa/BPLGUInfoCustomApp_MT6575_* $ANDROID_PRODUCT_OUT/symbols/

    if [ -e "$ANDROID_PRODUCT_OUT/symbols/MCDDLL.dll" ]; then
        rm "$ANDROID_PRODUCT_OUT/symbols/MCDDLL.dll"
    fi
    cp ./mediatek/custom/common/modem/${TARGET_PRODUCT}_hspa/MCDDLL.dll $ANDROID_PRODUCT_OUT/symbols/

    if [ -e "$ANDROID_PRODUCT_OUT/symbols/preloader_$TARGET_PRODUCT.elf" ]; then
        rm "$ANDROID_PRODUCT_OUT/symbols/preloader_$TARGET_PRODUCT.elf"
    fi
    cp ./mediatek/preloader/bin/preloader_$TARGET_PRODUCT.elf $ANDROID_PRODUCT_OUT/symbols/

    if [ -e "$ANDROID_PRODUCT_OUT/symbols/lk" ]; then
        rm "$ANDROID_PRODUCT_OUT/symbols/lk"
    fi
    cp ./bootable/bootloader/lk/build-$TARGET_PRODUCT/lk $ANDROID_PRODUCT_OUT/symbols/

    if [ -e "$ANDROID_PRODUCT_OUT/symbols-$BUILD_NUMBER.zip" ]; then
        rm $ANDROID_PRODUCT_OUT/symbols-$BUILD_NUMBER.zip
    fi

    cd  $ANDROID_PRODUCT_OUT
    zip -r symbols-$BUILD_NUMBER.zip symbols/
    croot

}

function zipimage()
{
    croot

    if [ -e "$ANDROID_PRODUCT_OUT/image-$BUILD_NUMBER.zip" ]; then
        rm "$ANDROID_PRODUCT_OUT/image-$BUILD_NUMBER.zip"
    fi

    cd $ANDROID_PRODUCT_OUT
    zip -D image-$BUILD_NUMBER.zip * -x\*.zip
    croot


}

function lgesign()
{
	# password can be entered with 1st prameter(ex: lgesign 123456)
	local PASSWORD

	if [ -n "$1" ]; then PASSWORD="-w";	fi

    # setp.1 Signing with LGE key
    build/tools/releasetools/sign_target_files_apks \
    -v $PASSWORD $1 -d vendor/lge/security out/dist/$TARGET_PRODUCT-target_files-*.zip $ANDROID_PRODUCT_OUT/signed_target_files.zip

    # step.2 make image from folder.
	if [ $? -ne 0 ]; then return 1; fi

    build/tools/releasetools/img_from_target_files $ANDROID_PRODUCT_OUT/signed_target_files.zip $ANDROID_PRODUCT_OUT/signed_imgs.zip

    # setp.3 unzip and move it.
    unzip $ANDROID_PRODUCT_OUT/signed_imgs.zip -d ./out/tmp > /dev/null

	if [ $? -ne 0 ]; then return 1; fi
	
    for NAME in $(ls ./out/tmp | grep 'img'); do
        rm $ANDROID_PRODUCT_OUT/$NAME 2>/dev/null
        mv out/tmp/$NAME $ANDROID_PRODUCT_OUT/$NAME 2>/dev/null
    done

    # step.4 boot.
	if [ $? -ne 0 ]; then return 1; fi

    vendor/lge/lge_security/secure_boot/tools_layer/security_tool boot $ANDROID_PRODUCT_OUT/boot.img lock md5
    vendor/lge/lge_security/secure_boot/tools_layer/security_tool boot $ANDROID_PRODUCT_OUT/recovery.img lock md5
}

