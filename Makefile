SHELL := /bin/bash
node-command := xargs -n 1 -I file node file $(params)

default:
	@echo need a target name...

test: test-unit 

bench:
	@find benchmark -name "*-bench.js" | $(node-command)

test-unit:
	@find test/unit -name "test-*.js" | $(node-command)

.PHONY : test
