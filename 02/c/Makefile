cc : 
	nasm -I boot/ -o build/mbr.bin boot/mbr.S
	nasm -I boot/ -o build/loader.bin boot/loader.S
	dd if=/dev/zero of=build/xcore.img bs=1M count=10
	dd if=build/mbr.bin of=build/xcore.img bs=512 count=1 conv=notrunc
	dd if=build/loader.bin of=build/xcore.img bs=512 count=4 seek=1 conv=notrunc
clean : 
	rm build/*
