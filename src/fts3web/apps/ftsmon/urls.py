# Copyright notice:
# Copyright (C) Members of the EMI Collaboration, 2010.
#
# See www.eu-emi.eu for details on the copyright holders
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from django.conf.urls.defaults import patterns, url

urlpatterns = patterns('ftsmon.views',
    url(r'^$', 'jobs.jobIndex'),
    url(r'^jobs/?$', 'jobs.jobIndex'),
    url(r'^jobs/(?P<jobId>[a-fA-F0-9\-]+)$', 'jobs.jobDetails'),
    url(r'^queue$', 'queue.queue'),
    url(r'^staging', 'jobs.staging'),
    url(r'^configuration$', 'statistics.configurationAudit'),
    
    url(r'^stats$', 'statistics.overview'),
    url(r'^stats/servers$', 'statistics.servers'),
    url(r'^stats/pairs$', 'statistics.pairs'),
    url(r'^stats/vo$', 'statistics.pervo'),
    url(r'^stats/profiling$', 'statistics.profiling'),
    
    url(r'^json/uniqueSources/$', 'autocomplete.uniqueSources', {'archive': None}),
    url(r'^json/uniqueDestinations/$', 'autocomplete.uniqueDestinations', {'archive': None}),
    url(r'^json/uniqueVos/$', 'autocomplete.uniqueVos', {'archive': None}),
    url(r'^json/uniqueSources/(?P<archive>[a-z]*?)$', 'autocomplete.uniqueSources'),
    url(r'^json/uniqueDestinations/(?P<archive>[a-z]*)$', 'autocomplete.uniqueDestinations'),
    url(r'^json/uniqueVos/(?P<archive>[a-z]*)$', 'autocomplete.uniqueVos'),
    
    url(r'^plot/pie', 'plots.pie'),
    
    url(r'^optimizer/$', 'optimizer.optimizer'),
    url(r'^optimizer/detailed$', 'optimizer.optimizerDetailed'),
    
    url(r'^errors/$', 'errors.showErrors'),
    url(r'^errors/list$', 'errors.transfersWithError'),
    
    url(r'^archive/$', 'jobs.archiveJobIndex')
)
