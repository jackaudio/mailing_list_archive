/* -*- Mode: C ; c-basic-offset: 4 -*- */
/* gcc test.c -Wall -Wextra -o test -O1 && ./test ; echo $?
   should print 0, prints 10
   gcc known to be affected:

Target: x86_64-redhat-linux
Configured with: ../configure --prefix=/usr --mandir=/usr/share/man --infodir=/usr/share/info --with-bugurl=http://bugzilla.redhat.com/bugzilla --enable-bootstrap --enable-shared --enable-threads=posix --enable-checking=release --disable-build-with-cxx --disable-build-poststage1-with-cxx --with-system-zlib --enable-__cxa_atexit --disable-libunwind-exceptions --enable-gnu-unique-object --enable-linker-build-id --with-linker-hash-style=gnu --enable-languages=c,c++,objc,obj-c++,java,fortran,ada,go,lto --enable-plugin --enable-initfini-array --enable-java-awt=gtk --disable-dssi --with-java-home=/usr/lib/jvm/java-1.5.0-gcj-1.5.0.0/jre --enable-libgcj-multifile --enable-java-maintainer-mode --with-ecj-jar=/usr/share/java/eclipse-ecj.jar --disable-libjava-multilib --with-ppl --with-cloog --with-tune=generic --with-arch_32=i686 --build=x86_64-redhat-linux
Thread model: posix
gcc version 4.7.0 20120507 (Red Hat 4.7.0-5) (GCC) 
*/

union u
{
    int i;
    _Bool b;
};

void f(union u * vp, union u v)
{
    *vp = v;
}

int main()
{
    union u v;
    union u v1;
    union u v2;

    v.i = 10;
    f(&v1, v);

    v.b = 0;
    f(&v2, v);
    return v2.b;
}
