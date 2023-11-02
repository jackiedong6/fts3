/*
 * Copyright (c) CERN 2013-2015
 *
 * Copyright (c) Members of the EMI Collaboration. 2010-2013
 *  See  http://www.eu-emi.eu/partners for details on the copyright
 *  holders.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "DinicsMaximumFlow.h"

DinicsMaximumFlow::Edge::Edge(std::string from, std::string to, long long capacity)
        : from(from), to(to), residual(nullptr), flow(0), capacity(capacity) {}

bool DinicsMaximumFlow::Edge::isResidual() {
    return capacity == 0;
}

long long DinicsMaximumFlow::Edge::remainingCapacity() {
    return capacity - flow;
}

void DinicsMaximumFlow::Edge::augment(long long bottleNeck) {
    flow += bottleNeck;
    residual->flow -= bottleNeck;
}

std::string DinicsMaximumFlow::Edge::edgeToString(std::string s, std::string t) {
    std::string u = (from == s) ? "s" : ((from == t) ? "t" : (from));
    std::string v = (to == s) ? "s" : ((to == t) ? "t" : (to));
    return "Edge " + u + " -> " + v + " | flow = " + std::to_string(flow) +
           " | capacity = " + std::to_string(capacity) + " | is residual: " + (isResidual() ? "true" : "false");
}

const long long DinicsMaximumFlow::MaximumFlowSolver::INF = LLONG_MAX / 2;

DinicsMaximumFlow::MaximumFlowSolver::MaximumFlowSolver(int nodes, std::string source, std::string sink)
        : nodes(nodes), source(source), sink(sink), solved(false), maximumFlow(0) {
    initializeEmptyFlowGraph();
}

void DinicsMaximumFlow::MaximumFlowSolver::addEdge(std::string from, std::string to, long long capacity) {
    if (capacity <= 0) {
        throw std::invalid_argument("Forward edge capacity <= 0");
    }
    // From edge
    Edge* e1 = new Edge(from, to, capacity);
    // To Edge
    Edge* e2 = new Edge(to, from, capacity);
    e1->residual = e2;
    e2->residual = e1;

    graph[from].push_back(e1);
    graph[to].push_back(e2);
}

std::map<std::string, std::vector<DinicsMaximumFlow::Edge*>> DinicsMaximumFlow::MaximumFlowSolver::retrieveGraph() {
    run();
    return graph;
}

long long DinicsMaximumFlow::MaximumFlowSolver::retrieveMaximumFlow() {
    run();
    return maximumFlow;
}

void DinicsMaximumFlow::MaximumFlowSolver::initializeEmptyFlowGraph() {
    graph = std::map<std::string, std::vector<DinicsMaximumFlow::Edge*>>();
}

void DinicsMaximumFlow::MaximumFlowSolver::run() {
    if (solved) return;
    solve();
    solved = true;
}

DinicsMaximumFlow::Dinics::Dinics(int nodes, std::string source, std::string sink)
        : MaximumFlowSolver(nodes, source, sink) {
    level = std::map<std::string, int>();
}

void DinicsMaximumFlow::Dinics::solve() {
    std::map<std::string, int> next;
    while (bfs()) {
        while (true) {
            long long flow = dfs(source, next, INF);
            if (flow == 0) {
                break;
            }
            maximumFlow += flow;
        }
    }
}

bool DinicsMaximumFlow::Dinics::bfs() {
    // Initialize all levels to -1
    for (const auto& entry : graph) {
        level[entry.first] = -1;
    }
    std::queue<std::string> queue;
    queue.push(source);
    level[source] = 0;
    while (!queue.empty()) {
        std::string node = queue.front();
        queue.pop();
        for (Edge* edge : graph[node]) {
            long long edgeCapacity = edge->remainingCapacity();
            // Add node to next level of level graph if it has positive capacity, and it brings us closer to the sink
            if (edgeCapacity > 0 && level[edge->to] == -1) {
                level[edge->to] = level[node] + 1;
                queue.push(edge->to);
            }
        }
    }
    return level[sink] != -1;
}

long long DinicsMaximumFlow::Dinics::dfs(std::string curr, std::map<std::string, int> next, long long flow) {
    if (curr == sink) return flow;
    int numEdges = graph[curr].size();
    for (; next[curr] < numEdges; next[curr]++) {
        Edge* edge = graph[curr][next[curr]];
        long long edgeCapacity = edge->remainingCapacity();
        if (edgeCapacity > 0 && level[edge->to] == level[curr] + 1) {
            long long bottleNeck = dfs(edge->to, next, std::min(flow, edgeCapacity));
            if (bottleNeck > 0) {
                edge->augment(bottleNeck);
                return bottleNeck;
            }
        }
    }
    // blocking flow found
    return 0;
}

