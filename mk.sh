make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image -j8
if [[ "$?" = "0" ]]; then
	md5sum arch/arm64/boot/Image
	scp arch/arm64/boot/Image s2r:/boot/efi/xen/vmlinuz-3.18.0+
	ssh s2r md5sum /boot/efi/xen/vmlinuz-3.18.0+
	ssh s2r sync
	echo "synced in the remote machine"
fi


