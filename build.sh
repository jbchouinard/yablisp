gcc -Wall -g mpc.c -g repl.c -o build/repl -I/usr/include -L/usr/lib -leditline -lm
gcc -Wall mpc.c doge.c -o build/doge -I/usr/include -L/usr/lib -leditline -lm
