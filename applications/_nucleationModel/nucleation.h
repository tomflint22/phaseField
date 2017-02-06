/*
//structure representing each nucleus
struct nucleus{
    unsigned int index;
    dealii::Point<problemDIM> center;
    double radius;
    double seededTime, seedingTime;
    unsigned int seedingTimestep;
};

//vector of all nucleus seeded in the problem
std::vector<nucleus> nuclei;
*/

// =================================================================================
// Nucleation function
// =================================================================================
template <int dim>
void generalizedProblem<dim>::modifySolutionFields()
{
    //current time
    double t=this->currentTime;
    //current time step
    unsigned int inc=this->currentIncrement;
    //minimum grid spacing
    double dx=spanX/(std::pow(2.0,refineFactor)*(double)finiteElementDegree);
    double rand_val;
    //unsigned int count=0;
    
    //get the list of node points in the domain
    std::map<dealii::types::global_dof_index, dealii::Point<dim> > support_points;
    dealii::DoFTools::map_dofs_to_support_points (dealii::MappingQ1<dim>(), *this->dofHandlersSet[0], support_points);
    //fields
    vectorType* n=this->solutionSet[this->getFieldIndex("n")];
    vectorType* c=this->solutionSet[this->getFieldIndex("c")];
    
    if ( (inc <= timeIncrements) && (inc % skipNucleationSteps == 0) ){
        
        //vector of all the NEW nuclei seeded in this time step
        std::vector<nucleus> newnuclei;
        
        //MPI INITIALIZATON
        int numProcs=Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);
        int thisProc=Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);
        int recvnonucs = 0;
        int currnonucs;
        
        if (numProcs < 2) {
            fprintf(stderr,"Requires at least two processes.\n");
            exit(-1);
        }
        
        if (thisProc > 0){
            //MPI SECTION TO RECIEVE INFORMATION FROM THE PREVIOUS THREAD
            currnonucs = nuclei.size();
            MPI_Recv(&recvnonucs, 1, MPI_INT, thisProc-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (recvnonucs > 0){
                
                //Creating vectors of each quantity in nuclei. Each numbered acording to the tags used for MPI_Send/MPI_Recv
                //1 - index
                std::vector<unsigned int> r_index(recvnonucs,0);
                //2 - "x" componenet of center
                std::vector<double> r_center_x(recvnonucs,0.0);
                //3 - "y" componenet of center
                std::vector<double> r_center_y(recvnonucs,0.0);
                //4 - radius
                std::vector<double> r_radius(recvnonucs,0.0);
                //5 - seededTime
                std::vector<double> r_seededTime(recvnonucs,0.0);
                //6 - seedingTime
                std::vector<double> r_seedingTime(recvnonucs,0.0);
                //7 - seedingTimestep
                std::vector<unsigned int> r_seedingTimestep(recvnonucs,0);
                
                //Recieve vectors from previous thread
                MPI_Recv(&r_index[0], recvnonucs, MPI_UNSIGNED, thisProc-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_center_x[0], recvnonucs, MPI_DOUBLE, thisProc-1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_center_y[0], recvnonucs, MPI_DOUBLE, thisProc-1, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_radius[0], recvnonucs, MPI_DOUBLE, thisProc-1, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seededTime[0], recvnonucs, MPI_DOUBLE, thisProc-1, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seedingTime[0], recvnonucs, MPI_DOUBLE, thisProc-1, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seedingTimestep[0], recvnonucs, MPI_UNSIGNED, thisProc-1, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                //Loop to store info in vectors onto the nuclei structure
                if (recvnonucs > currnonucs){
                    for (int jnuc=currnonucs; jnuc<=recvnonucs-1; jnuc++){
                        nucleus* temp = new nucleus;
                        temp->index=r_index[jnuc];
                        dealii::Point<dim> r_center;
                        r_center[0]=r_center_x[jnuc];
                        r_center[1]=r_center_y[jnuc];
                        temp->center=r_center;
                        temp->radius=r_radius[jnuc];
                        temp->seededTime=r_seededTime[jnuc];
                        temp->seedingTime = r_seedingTime[jnuc];
                        temp->seedingTimestep = r_seedingTimestep[jnuc];
                        nuclei.push_back(*temp);
                        newnuclei.push_back(*temp);
                    }
                }
                
            }
            //END OF MPI BLOCK
        }
        
        if (thisProc == 0){
            std::cout << "nucleation attempt" << std::endl;
        }
        //Better random no. generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> distr(0.0,1.0);
        //add nuclei based on concentration field values
        //loop over all points in the domain
        for (typename std::map<dealii::types::global_dof_index, dealii::Point<dim> >::iterator it=support_points.begin(); it!=support_points.end(); ++it){
            unsigned int dof=it->first;
            //set only local owned values of the parallel vector
            if (n->locally_owned_elements().is_element(dof)){
                dealii::Point<dim> nodePoint=it->second;
                //Excluding regions near the borders
                if ((nodePoint[0] > borderreg) && (nodePoint[0] < spanX-borderreg) && (nodePoint[1] > borderreg) && (nodePoint[1] < spanY-borderreg)){
                    double nValue=(*n)(dof);
                    double cValue=(*c)(dof);
                    //Compute random no. between 0 and 1 (old method)
                    //rand_val = (rand() % rand_scale)/((double)rand_scale);
                    //Better random no. generator
                    rand_val=distr(gen);
                    //Compute nucleation rate
                    double J=k1*exp(-k2/(cValue-calmin));
                    //We need element volume (or area in 2D)
                    double Prob=1.0-exp(-J*timeStep*((double)skipNucleationSteps)*dx*dx);
                    if (rand_val <= Prob){
                        //std::cout << "random value " << rand_val << ", probability " << Prob << std::endl;
                        //loop over all existing nuclei to check if they are in the vicinity
                        bool isClose=false;
                        for (std::vector<nucleus>::iterator thisNuclei=nuclei.begin(); thisNuclei!=nuclei.end(); ++thisNuclei){
                            if (thisNuclei->center.distance(nodePoint)<minDistBetwenNuclei){
                                isClose=true;
                            }
                        }
                        if (!isClose){
                            std::cout << "Nucleation event. Nucleus no. " << nuclei.size()+1 << std::endl;
                            std::cout << "nucleus center " << nodePoint << std::endl;
                            nucleus* temp = new nucleus;
                            temp->index=nuclei.size();
                            temp->center=nodePoint;
                            temp->radius=n_radius;
                            temp->seededTime=this->currentTime;
                            temp->seedingTime = t_hold;
                            temp->seedingTimestep = inc;
                            nuclei.push_back(*temp);
                            newnuclei.push_back(*temp);
                        }
                    }
                }
            }
        }
        
        //MPI BLOCK
        int nonucs=nuclei.size();
        //All threads except the last one send info to next thread
        if (thisProc < numProcs-1){
            //MPI SECTION TO SEND INFORMATION TO THE NEXT THREAD
            //Sending local no. of nuclei
            MPI_Send(&nonucs, 1, MPI_INT, thisProc+1, 0, MPI_COMM_WORLD);
            if (nonucs > 0){
                //Creating vectors of each quantity in nuclei. Each numbered acording to the tags used for MPI_Send/MPI_Recv
                //1 - index
                std::vector<unsigned int> s_index;
                //2 - "x" componenet of center
                std::vector<double> s_center_x;
                //3 - "y" componenet of center
                std::vector<double> s_center_y;
                //4 - radius
                std::vector<double> s_radius;
                //5 - seededTime
                std::vector<double> s_seededTime;
                //6 - seedingTime
                std::vector<double> s_seedingTime;
                //7 - seedingTimestep
                std::vector<unsigned int> s_seedingTimestep;
                
                //Loop to store info of all nuclei into vectors
                for (std::vector<nucleus>::iterator thisNuclei=nuclei.begin(); thisNuclei!=nuclei.end(); ++thisNuclei){
                    s_index.push_back(thisNuclei->index);
                    dealii::Point<problemDIM> s_center=thisNuclei->center;
                    s_center_x.push_back(s_center[0]);
                    s_center_y.push_back(s_center[1]);
                    s_radius.push_back(thisNuclei->radius);
                    s_seededTime.push_back(thisNuclei->seededTime);
                    s_seedingTime.push_back(thisNuclei->seedingTime);
                    s_seedingTimestep.push_back(thisNuclei->seedingTimestep);
                }
                //Send vectors to next thread
                MPI_Send(&s_index[0], nonucs, MPI_UNSIGNED, thisProc+1, 1, MPI_COMM_WORLD);
                MPI_Send(&s_center_x[0], nonucs, MPI_DOUBLE, thisProc+1, 2, MPI_COMM_WORLD);
                MPI_Send(&s_center_y[0], nonucs, MPI_DOUBLE, thisProc+1, 3, MPI_COMM_WORLD);
                MPI_Send(&s_radius[0], nonucs, MPI_DOUBLE, thisProc+1, 4, MPI_COMM_WORLD);
                MPI_Send(&s_seededTime[0], nonucs, MPI_DOUBLE, thisProc+1, 5, MPI_COMM_WORLD);
                MPI_Send(&s_seedingTime[0], nonucs, MPI_DOUBLE, thisProc+1, 6, MPI_COMM_WORLD);
                MPI_Send(&s_seedingTimestep[0], nonucs, MPI_UNSIGNED, thisProc+1, 7, MPI_COMM_WORLD);
            }
            //END OF MPI SECTION
        }
        
        //Last thread (N-1) sends info to first thread 0
        if (thisProc == numProcs-1){
            //MPI SECTION TO SEND INFORMATION TO THREAD 0
            //Sending local no. of nuclei
            MPI_Send(&nonucs, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            if (nonucs > 0){
                //Creating vectors of each quantity in nuclei. Each numbered acording to the tags used for MPI_Send/MPI_Recv
                //1 - index
                std::vector<unsigned int> s_index;
                //2 - "x" componenet of center
                std::vector<double> s_center_x;
                //3 - "y" componenet of center
                std::vector<double> s_center_y;
                //4 - radius
                std::vector<double> s_radius;
                //5 - seededTime
                std::vector<double> s_seededTime;
                //6 - seedingTime
                std::vector<double> s_seedingTime;
                //7 - seedingTimestep
                std::vector<unsigned int> s_seedingTimestep;
                
                //Loop to store info of all nuclei into vectors
                for (std::vector<nucleus>::iterator thisNuclei=nuclei.begin(); thisNuclei!=nuclei.end(); ++thisNuclei){
                    s_index.push_back(thisNuclei->index);
                    dealii::Point<problemDIM> s_center=thisNuclei->center;
                    s_center_x.push_back(s_center[0]);
                    s_center_y.push_back(s_center[1]);
                    s_radius.push_back(thisNuclei->radius);
                    s_seededTime.push_back(thisNuclei->seededTime);
                    s_seedingTime.push_back(thisNuclei->seedingTime);
                    s_seedingTimestep.push_back(thisNuclei->seedingTimestep);
                }
                //Send vectors to next thread
                MPI_Send(&s_index[0], nonucs, MPI_UNSIGNED, 0, 1, MPI_COMM_WORLD);
                MPI_Send(&s_center_x[0], nonucs, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
                MPI_Send(&s_center_y[0], nonucs, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);
                MPI_Send(&s_radius[0], nonucs, MPI_DOUBLE, 0, 4, MPI_COMM_WORLD);
                MPI_Send(&s_seededTime[0], nonucs, MPI_DOUBLE, 0, 5, MPI_COMM_WORLD);
                MPI_Send(&s_seedingTime[0], nonucs, MPI_DOUBLE, 0, 6, MPI_COMM_WORLD);
                MPI_Send(&s_seedingTimestep[0], nonucs, MPI_UNSIGNED, 0, 7, MPI_COMM_WORLD);
            }
            //END OF MPI SECTION
        }
        
        if (thisProc == 0){
            //MPI SECTION TO RECIEVE INFORMATION FROM THREAD N-1
            currnonucs = nuclei.size();
            MPI_Recv(&recvnonucs, 1, MPI_INT, numProcs-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            //Recieve vector nuclei
            if (recvnonucs > 0){
                
                //Creating vectors of each quantity in nuclei. Each numbered acording to the tags used for MPI_Send/MPI_Recv
                //1 - index
                std::vector<unsigned int> r_index(recvnonucs,0);
                //2 - "x" componenet of center
                std::vector<double> r_center_x(recvnonucs,0.0);
                //3 - "y" componenet of center
                std::vector<double> r_center_y(recvnonucs,0.0);
                //4 - radius
                std::vector<double> r_radius(recvnonucs,0.0);
                //5 - seededTime
                std::vector<double> r_seededTime(recvnonucs,0.0);
                //6 - seedingTime
                std::vector<double> r_seedingTime(recvnonucs,0.0);
                //7 - seedingTimestep
                std::vector<unsigned int> r_seedingTimestep(recvnonucs,0);
                
                //Recieve vectors from previous thread
                MPI_Recv(&r_index[0], recvnonucs, MPI_UNSIGNED, numProcs-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_center_x[0], recvnonucs, MPI_DOUBLE, numProcs-1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_center_y[0], recvnonucs, MPI_DOUBLE, numProcs-1, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_radius[0], recvnonucs, MPI_DOUBLE, numProcs-1, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seededTime[0], recvnonucs, MPI_DOUBLE, numProcs-1, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seedingTime[0], recvnonucs, MPI_DOUBLE, numProcs-1, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seedingTimestep[0], recvnonucs, MPI_UNSIGNED, numProcs-1, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                //Loop to store info in vectors onto the nuclei structure
                if (recvnonucs > currnonucs){
                    for (int jnuc=currnonucs; jnuc<=recvnonucs-1; jnuc++){
                        nucleus* temp = new nucleus;
                        temp->index=r_index[jnuc];
                        dealii::Point<dim> r_center;
                        r_center[0]=r_center_x[jnuc];
                        r_center[1]=r_center_y[jnuc];
                        temp->center=r_center;
                        temp->radius=r_radius[jnuc];
                        temp->seededTime=r_seededTime[jnuc];
                        temp->seedingTime = r_seedingTime[jnuc];
                        temp->seedingTimestep = r_seedingTimestep[jnuc];
                        nuclei.push_back(*temp);
                        newnuclei.push_back(*temp);
                    }
                }
            }
            //END OF MPI SECTON
        }
        
        //Barrier for all threads
        MPI_Barrier(MPI_COMM_WORLD);
        
        //Update number of nuclei updated for all threads
        nonucs = nuclei.size();
        
        if (thisProc == 0){
            //MPI SECTION TO BROADCAST INFORMATION FROM THREAD 0 TO ALL OTHER THREADS
            //Sending local no. of nuclei
            for (int jproc=1; jproc<=numProcs-1; jproc++){
                MPI_Send(&nonucs, 1, MPI_INT, jproc, 0, MPI_COMM_WORLD);
            }
            if (nonucs > 0){
                //Creating vectors of each quantity in nuclei. Each numbered acording to the tags used for MPI_Send/MPI_Recv
                //1 - index
                std::vector<unsigned int> s_index;
                //2 - "x" componenet of center
                std::vector<double> s_center_x;
                //3 - "y" componenet of center
                std::vector<double> s_center_y;
                //4 - radius
                std::vector<double> s_radius;
                //5 - seededTime
                std::vector<double> s_seededTime;
                //6 - seedingTime
                std::vector<double> s_seedingTime;
                //7 - seedingTimestep
                std::vector<unsigned int> s_seedingTimestep;
                
                //Loop to store info of all nuclei into vectors
                for (std::vector<nucleus>::iterator thisNuclei=nuclei.begin(); thisNuclei!=nuclei.end(); ++thisNuclei){
                    s_index.push_back(thisNuclei->index);
                    dealii::Point<problemDIM> s_center=thisNuclei->center;
                    s_center_x.push_back(s_center[0]);
                    s_center_y.push_back(s_center[1]);
                    s_radius.push_back(thisNuclei->radius);
                    s_seededTime.push_back(thisNuclei->seededTime);
                    s_seedingTime.push_back(thisNuclei->seedingTime);
                    s_seedingTimestep.push_back(thisNuclei->seedingTimestep);
                }
                //Send vectors to next thread
                for (int jproc=1; jproc<=numProcs-1; jproc++){
                    MPI_Send(&s_index[0], nonucs, MPI_UNSIGNED, jproc, 1, MPI_COMM_WORLD);
                    MPI_Send(&s_center_x[0], nonucs, MPI_DOUBLE, jproc, 2, MPI_COMM_WORLD);
                    MPI_Send(&s_center_y[0], nonucs, MPI_DOUBLE, jproc, 3, MPI_COMM_WORLD);
                    MPI_Send(&s_radius[0], nonucs, MPI_DOUBLE, jproc, 4, MPI_COMM_WORLD);
                    MPI_Send(&s_seededTime[0], nonucs, MPI_DOUBLE, jproc, 5, MPI_COMM_WORLD);
                    MPI_Send(&s_seedingTime[0], nonucs, MPI_DOUBLE, jproc, 6, MPI_COMM_WORLD);
                    MPI_Send(&s_seedingTimestep[0], nonucs, MPI_UNSIGNED, jproc, 7, MPI_COMM_WORLD);
                }
            }
            //END OF MPI SECTION
        }
        
        if (thisProc > 0){
            //MPI SECTION TO RECIEVE INFORMATION FROM THE THREAD 0
            MPI_Recv(&recvnonucs, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (recvnonucs > 0){
                
                //Creating vectors of each quantity in nuclei. Each numbered acording to the tags used for MPI_Send/MPI_Recv
                //1 - index
                std::vector<unsigned int> r_index(recvnonucs,0);
                //2 - "x" componenet of center
                std::vector<double> r_center_x(recvnonucs,0.0);
                //3 - "y" componenet of center
                std::vector<double> r_center_y(recvnonucs,0.0);
                //4 - radius
                std::vector<double> r_radius(recvnonucs,0.0);
                //5 - seededTime
                std::vector<double> r_seededTime(recvnonucs,0.0);
                //6 - seedingTime
                std::vector<double> r_seedingTime(recvnonucs,0.0);
                //7 - seedingTimestep
                std::vector<unsigned int> r_seedingTimestep(recvnonucs,0);
                
                //Recieve vectors from previous thread
                MPI_Recv(&r_index[0], recvnonucs, MPI_UNSIGNED, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_center_x[0], recvnonucs, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_center_y[0], recvnonucs, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_radius[0], recvnonucs, MPI_DOUBLE, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seededTime[0], recvnonucs, MPI_DOUBLE, 0, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seedingTime[0], recvnonucs, MPI_DOUBLE, 0, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&r_seedingTimestep[0], recvnonucs, MPI_UNSIGNED, 0, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                //Loop to store info in vectors onto the nuclei structure
                if (recvnonucs > nonucs){
                    for (int jnuc=nonucs; jnuc<=recvnonucs-1; jnuc++){
                        nucleus* temp = new nucleus;
                        temp->index=r_index[jnuc];
                        dealii::Point<dim> r_center;
                        r_center[0]=r_center_x[jnuc];
                        r_center[1]=r_center_y[jnuc];
                        temp->center=r_center;
                        temp->radius=r_radius[jnuc];
                        temp->seededTime=r_seededTime[jnuc];
                        temp->seedingTime = r_seedingTime[jnuc];
                        temp->seedingTimestep = r_seedingTimestep[jnuc];
                        nuclei.push_back(*temp);
                        newnuclei.push_back(*temp);
                    }
                }
            }
            //END OF MPI BLOCK
        }
        //Barrier for all threads
        MPI_Barrier(MPI_COMM_WORLD);
        
        //Seeding nucleus section
        //Looping over all nodes
        for (typename std::map<dealii::types::global_dof_index, dealii::Point<dim> >::iterator it=support_points.begin(); it!=support_points.end(); ++it){
            unsigned int dof=it->first;
            //set only local owned values of the parallel vector
            if (n->locally_owned_elements().is_element(dof)){
                dealii::Point<dim> nodePoint=it->second;
                //Looping over all nuclei seeded in this iteration
                for (std::vector<nucleus>::iterator thisNuclei=newnuclei.begin(); thisNuclei!=newnuclei.end(); ++thisNuclei){
                    dealii::Point<dim> center=thisNuclei->center;
                    double r=nodePoint.distance(center);
                    if (r<=opfreeze_radius){
                        (*n)(dof)=0.5*(1.0-std::tanh((r-n_radius)/interface_coeff));
                    }
                }
            }
        }
    }
}
