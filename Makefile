current_dir:=${CURDIR}
package_dir:=${CURDIR}/package

targets = alpine debian
libs = libklusolvex.so libdss_capi.so librmqpush.so

default: debian

klu:
	make -C ../klusolve 
	cp ../klusolve/lib/linux_x64/libklusolvex.so ${current_dir}/lib/linux_x64/

rmqpush:
	make -C ./zepben-extensions/ $@
	cp ./zepben-extensions/src/include/rmqpush.h include/rmqpush.h
	cp ./zepben-extensions/lib/*.so lib/linux_x64/

debian: debian-builder rmqpush
alpine: alpine-builder

debian-builder:
	podman build -f=Dockerfile.$@ -t ghcr.io/zepben/dss-capi-builder:latest

$(targets): 
	podman run -v ${current_dir}:/build/dss_capi -w /build/dss_capi ghcr.io/zepben/dss-capi-builder:latest build/build_linux_x64.sh

ci: rmqpush 
	build/build_linux_x64.sh

package: lib/linux_x64/libdss_capi.so 
	if [ -d ${package_dir} ];  then rm -rf ${package_dir}; fi
	mkdir package
	cd ${current_dir}/lib/linux_x64/ && cp ${libs} ${package_dir} 
	cp ${current_dir}/include/rmqpush.h ${package_dir}
	cd ${package_dir} && tar cjvf dss-libs.bz2 *

runner: dss-runner.c
	cp zepben-extensions/src/include/rmqpush.h include/rmqpush.h
	gcc -Llib/linux_x64 -ldss_capi -lrabbitmq -lrmqpush -o dss-runner dss-runner.c

clean:
	make -C ./zepben-extensions/ clean
	rm -rf lib/linux_x64/libdss*.so

cleanall:
	make -C ./zepben-extensions/ clean
	rm -rf include/rmqpush.h
	rm -rf lib/linux_x64/lib*.so
	rm -rf ${current_dir}/package
	cp ../rabbitmq-c/librabbitmq/librabbitmq.so.0.14.0 lib/linux_x64/librabbitmq.so
	cp ../klusolve/lib/linux_x64/libklusolvex.so ${current_dir}/lib/linux_x64/
