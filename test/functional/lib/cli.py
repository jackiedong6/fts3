import config
import json
import fts3
import logging
import os
import subprocess
import tempfile
import time


class Cli:

	def submit(self, transfers, extraArgs = []):
		"""
		Spawns a transfer and returns the job ID
		"""
		# Build the submission file
		submission = tempfile.NamedTemporaryFile(delete = False, suffix = '.submission')
		submission.write(json.dumps({'Files': transfers}))
		submission.close()

		# Spawn the transfer
		cmdArray = ['fts-transfer-submit',
                    '-s', config.Fts3Endpoint,
                    '--job-metadata', config.TestLabel,
					'--new-bulk-format', '-f', submission.name] + extraArgs
		logging.debug("Spawning %s" % ' '.join(cmdArray))
		proc = subprocess.Popen(cmdArray, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
		rcode = proc.wait()
		if rcode != 0:
			logging.error(proc.stdout.read())
			logging.error(proc.stderr.read())
			raise Exception("fts-transfer-submit failed with exit code %d" % rcode)
		jobId = proc.stdout.read().strip()

		os.unlink(submission.name)

		return jobId


	def getJobState(self, jobId):
		cmdArray = ['fts-transfer-status', '-s', config.Fts3Endpoint, jobId]
		logging.debug("Spawning %s" % ' '.join(cmdArray))
		proc = subprocess.Popen(cmdArray, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
		rcode = proc.wait()
		if rcode != 0:
			logging.error(proc.stdout.read())
			logging.error(proc.stderr.read())
			raise Exception("fts-transfer-status failed with exit code %d" % rcode)
		state = proc.stdout.read().strip()
		return state


	def poll(self, jobId):
		state = self.getJobState(jobId)
		remaining = config.Timeout
		while state not in fts3.JobTerminalStates:
			logging.debug("%s %s" % (jobId, state))
			time.sleep(config.PollInterval)
			remaining -= config.PollInterval
			state = self.getJobState(jobId)
			if remaining <= 0:
				self.error("Timeout expired, cancelling job")
				self.cancel(jobId)
				raise Exception("Timeout expired while polling")

		return state


	def cancel(self, jobId):
		cmdArray = ['fts-transfer-cancel', '-s', config.Fts3Endpoint, jobId]
		logging.debug("Spawning %s" % ' '.join(cmdArray))
		proc = subprocess.Popen(cmdArray, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
		rcode = proc.wait()
		if rcode != 0:
			logging.error(proc.stdout.read())
			logging.error(proc.stderr.read())
			raise Exception("fts-transfer-cancel failed with exit code %d" % rcode)

