cc : 
	nasm -I boot/ -o build/mbr.bin boot/mbr.S
	nasm -I boot/ -o build/loader.bin boot/loader.S
	gcc -m32 -c -o build/main.o kernel/main.c
	ld -melf_i386 -Ttext 0xc0001500 -e main -o build/kernel.bin build/main.o
	dd if=/dev/zero of=build/xcore.img bs=1M count=10
	dd if=build/mbr.bin of=build/xcore.img bs=512 count=1 conv=notrunc
	dd if=build/loader.bin of=build/xcore.img bs=512 count=4 seek=1 conv=notrunc
	dd if=build/kernel.bin of=build/xcore.img bs=512 count=200 seek=5 conv=notrunc
clean : 
	rm build/*