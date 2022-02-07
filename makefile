cc=g++
prom=webser
src=main.cpp block_queue.h http_con.h http_con.cpp locker.h log.cpp log.h lst_timer.cpp \
	lst_timer.h threadpool.h
order=-lpthread

$(prom):$(src)
	$(cc) -o $(prom) $(src) $(order)


