#include <iostream>
#include <omp.h>
#include <algorithm> 
#include "graphGenerator.cpp"
#include <numeric>
#include <mpi.h>
#include <math.h>

int RANK;
int PROCS;

using namespace std;

void getAllChildren(vector<vector<int>> &g, vector<int> &frontier, vector<int> &children, int* distances, int k){
	//printf("%d\n", frontier.size());
	for(int i=0; i<frontier.size(); i++){
		int node = frontier[i];
		for(int j=0; j<g[node].size(); j++){
			int n = g[node][j];
			if(distances[n] == -1){
				distances[n] = k;
				children.push_back(n);
			}
		}
	}
}

void getAllChildrenParallel(vector<vector<int>> &g, vector<int> &frontier, vector<int> &children, int* distances, int k){
	int count = 0;
	vector<int> local_children = vector<int>{};
	#pragma omp parallel private(local_children) num_threads(20)
	{
	#pragma omp for nowait
        for(int i=0; i<frontier.size(); i++){
                int node = frontier[i];
                for(int j=0; j<g[node].size(); j++){
                        int n = g[node][j];
                        if(distances[n] == -1){
                                distances[n] = k;
                                local_children.push_back(n);
                        }
                }
        }
	#pragma omp critical
	move(local_children.begin(), local_children.end(), back_inserter(children));
	}
}

int* bfs(vector<vector<int>> g, int source){
	vector<int> frontier = {source};
	vector<int> children = vector<int>{};
	
	int *distances = new int[g.size()];
	for(int i=0; i<g.size(); i++){
		distances[i] = -1;
	}
	
	int k = 1;
	while(frontier.size() > 0){
		getAllChildren(g, frontier, children, distances, k++);
		frontier = children;
		children = {};
	}
	return distances;
}

int* bfsParallel(vector<vector<int>> g, int source){
        vector<int> frontier = {source};
        vector<int> children = vector<int>{};

        int *distances = new int[g.size()];
        for(int i=0; i<g.size(); i++){
                distances[i] = -1;
        }

        int k = 1;
        while(frontier.size() > 0){
                getAllChildrenParallel(g, frontier, children, distances, k++);
                frontier = children;
                children = {};
        }
        return distances;
}

void serialBfsApsp(vector<vector<int>> g){
	int chunkSize = ceil(1.0*g.size()/PROCS);
	int startSrc = chunkSize * RANK;
        int endSrc   = chunkSize * (RANK+1);
        if(endSrc > g.size())
                endSrc = endSrc % g.size();
	int currSize = endSrc-startSrc;
	//printf("%d %d %d", startSrc, endSrc, currSize);
	float* diameter = new float[currSize];
	float* distance = new float[currSize];
	
	for(int i=0; i<currSize; i++){
		int* distances = bfs(g, startSrc+i);	
		diameter[i] = *max_element(distances, distances + g.size());
		distance[i] = accumulate(distances, distances + g.size(), 0.0) / g.size();
	}
	cout << "Proc:" << RANK << "  "<< "Max diameter: " << *max_element(diameter, diameter + currSize) << endl;
	cout << "Proc:" << RANK << "  " << "Avg distance: " << (accumulate(distance, distance + currSize, 0.0) / currSize) << endl;
}

void parallelBfsApsp(vector<vector<int>> g){
	int chunkSize = ceil(1.0*g.size()/PROCS);
	int startSrc = chunkSize * RANK;
        int endSrc   = chunkSize * (RANK+1);
        if(endSrc > g.size())
                endSrc = endSrc % g.size();
	int currSize = endSrc-startSrc;
	float* diameter = new float[currSize];
	float* distance = new float[currSize];
	
	#pragma omp parallel for num_threads(20)
	for(int i=0; i<currSize; i++){
                int* distances = bfsParallel(g, startSrc+i);
		diameter[i] = *max_element(distances, distances + g.size());
		distance[i] = accumulate(distances, distances + g.size(), 0.0) / g.size();
	}
	cout << "Proc:" << RANK << "  " << "Max diameter: " << *max_element(diameter, diameter + currSize) << endl;
	cout << "Proc:" << RANK << "  " << "Avg distance: " << (accumulate(distance, distance + currSize, 0.0) / currSize) << endl;
}



int main(int argc, char *argv[]) {
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &PROCS);
	MPI_Comm_rank(MPI_COMM_WORLD, &RANK);
	string fileName = "n1024d4.random.edges"; 
	int order = 1024;
	
	vector<vector<int>> g = getAdjacencyListVector(fileName,order); 

	double tt = omp_get_wtime();
	serialBfsApsp(g);
	printf("sequential-bfs-apsp = %fs\n", omp_get_wtime() - tt);

	tt = omp_get_wtime();
	parallelBfsApsp(g);
	printf("parallel-bfs-apsp = %fs\n", omp_get_wtime() - tt);
	if(RANK == 0){
		printf("But I am the main dude. %d / %d \n", RANK, PROCS);
	}
	MPI_Finalize();
	return 0;
	
}
