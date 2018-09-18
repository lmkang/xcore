cc : 
	nasm -o build/mbr.bin boot/mbr.S
	dd if=/dev/zero of=build/xcore.img bs=1M count=10
	dd if=build/mbr.bin of=build/xcore.img bs=512 count=1 conv=notrunc
clean : 
	rm build/*
