.PHONY: test perf-budget memory-budget budget

test:
	$(MAKE) -C tests test

perf-budget:
	$(MAKE) -C tests .build/test_render_budget
	./tests/.build/test_render_budget

memory-budget:
	python3 scripts/check_memory_budget.py thermostat-debug.yaml

budget: perf-budget memory-budget
