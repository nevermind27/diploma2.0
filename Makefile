.PHONY: all clean install test run

all: routing_server storage_server client

routing_server:
	$(MAKE) -C routing_server

storage_server:
	$(MAKE) -C storage_server

client:
	$(MAKE) -C client install

install: all
	# Создание базы данных
	cd routing_server && ./db_creator.sh
	cd storage_server && ./db_creator.sh

test:
	$(MAKE) -C client test

run:
	# Запуск всех компонентов
	cd routing_server && ./routing_server &
	cd storage_server && ./storage_server &
	cd client && $(MAKE) run

clean:
	$(MAKE) -C routing_server clean
	$(MAKE) -C storage_server clean
	$(MAKE) -C client clean 