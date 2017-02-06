// List of variables and residual equations for the coupled Allen-Cahn/Cahn-Hilliard example application

// =================================================================================
// Define the variables in the model
// =================================================================================
// The number of variables
#define num_var 2

// The names of the variables, whether they are scalars or vectors and whether the
// governing eqn for the variable is parabolic or elliptic
#define variable_name {"c", "n"}
#define variable_type {"SCALAR","SCALAR"}
#define variable_eq_type {"PARABOLIC","PARABOLIC"}

// Flags for whether the value, gradient, and Hessian are needed in the residual eqns
#define need_val {true, true}
#define need_grad {true, true}
#define need_hess {false, false}

// Flags for whether the residual equation has a term multiplied by the test function
// (need_val_residual) and/or the gradient of the test function (need_grad_residual)
#define need_val_residual {true, true}
#define need_grad_residual {true, true}

// =================================================================================
// Define the Model and residual equations
// =================================================================================
// The model free energy equations and expressions for the residual equations
// can be set here. For simple cases, the entire residual equation can be written
// here. For more complex cases with loops or conditional statements, residual
// equations (or parts of residual equations) can be written below in "residualRHS".

// Free energy for each phase and their first and second derivatives
#define faV (A0+A2*(c_alpha-calmin)*(c_alpha-calmin))
#define facV (2.0*A2*(c_alpha-calmin))
#define faccV (2.0*A2)
#define fbV (B0+B2*(c_beta-cbtmin)*(c_beta-cbtmin))
#define fbcV (2.0*B2*(c_beta-cbtmin))
#define fbccV (2.0*B2)

// Interpolation function and its derivative
#define hV (3.0*n*n - 2.0*n*n*n)
#define hnV (6.0*n - 6.0*n*n)

// KKS model c_alpha and c_beta as a function of c and h
#define c_alpha ((B2*(c-cbtmin*hV) + A2*calmin*hV)/(A2*hV+B2*(1.0-hV)))
#define c_beta ((A2*(c-calmin*(1.0-hV))+B2*cbtmin*(1.0-hV))/(A2*hV+B2*(1.0-hV)))

// Double-Well function (can be used to tune the interfacial energy)
#define fbarrierV (n*n - 2.0*n*n*n + n*n*n*n)
#define fbarriernV (2.0*n - 6.0*n*n + 4.0*n*n*n)

// Residual equations
// For concentration
#define term_muxV (cx + (c_alpha - c_beta)*hnV*nx)
#define rcV   (c)
#define rcxV  (constV(-McV*timeStep)*term_muxV)
//For order parameter (gamma is a variable order parameter mobility factor)
#define rnV   (n-constV(timeStep*MnV)*gamma*((fbV-faV)*hnV - (c_beta-c_alpha)*fbcV*hnV + W*fbarriernV))
#define rnxV  (constV(-timeStep*KnV*MnV)*gamma*nx)


// =================================================================================
// Define the global variable containing the information of every nuclei
// =================================================================================
// Definition of global variable nucleus containing the information of every nuclei
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

// =================================================================================
// residualRHS
// =================================================================================
// This function calculates the residual equations for each variable. It takes
// "modelVariablesList" as an input, which is a list of the value and derivatives of
// each of the variables at a specific quadrature point. The (x,y,z) location of
// that quadrature point is given by "q_point_loc".
// This function also calculates the factor (gamma) that multiplies the order parameter mobility
// during the hold time after each nucleus has been seeded.
// The function outputs
// "modelResidualsList", a list of the value and gradient terms of the residual for
// each residual equation. The index for each variable in these lists corresponds to
// the order it is defined at the top of this file (starting at 0).
template <int dim>
void generalizedProblem<dim>::residualRHS(const std::vector<modelVariable<dim>> & modelVariablesList,
												std::vector<modelResidual<dim>> & modelResidualsList,
												dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const {

double time = this->currentTime;
double dx=spanX/std::pow(2.0,refineFactor);

//Calculation of mobility factor, gamma
dealii::VectorizedArray<double> gamma = constV(1.0);
dealii::VectorizedArray<double> x= q_point_loc[0];
dealii::VectorizedArray<double> y= q_point_loc[1];

for (std::vector<nucleus>::iterator thisNuclei=nuclei.begin(); thisNuclei!=nuclei.end(); ++thisNuclei){
    dealii::Point<problemDIM> r=thisNuclei->center;
    double nucendtime = thisNuclei->seededTime + thisNuclei->seedingTime;
    dealii::VectorizedArray<double> spacearg=(x-constV(r[0]))*(x-constV(r[0]))+(y-constV(r[1]))*(y-constV(r[1]));
    spacearg=std::sqrt(spacearg);
    spacearg=(spacearg - constV(opfreeze_radius))/constV(dx);
    dealii::VectorizedArray<double> timearg=constV(time-nucendtime)/constV(timeStep);
    dealii::VectorizedArray<double> spacefactor=constV(0.5)-constV(0.5)*spacearg/(std::abs(spacearg)+epsil);
    dealii::VectorizedArray<double> timefactor=constV(0.5)-constV(0.5)*timearg/(std::abs(timearg)+epsil);
    dealii::VectorizedArray<double> localgamma= constV(1.0)-spacefactor*timefactor;
    gamma=gamma*localgamma;
}

// The concentration and its derivatives (names here should match those in the macros above)
scalarvalueType c = modelVariablesList[0].scalarValue;
scalargradType cx = modelVariablesList[0].scalarGrad;

// The order parameter and its derivatives (names here should match those in the macros above)
scalarvalueType n = modelVariablesList[1].scalarValue;
scalargradType nx = modelVariablesList[1].scalarGrad;

// Residuals for the equation to evolve the concentration (names here should match those in the macros above)
modelResidualsList[0].scalarValueResidual = rcV;
modelResidualsList[0].scalarGradResidual = rcxV;

// Residuals for the equation to evolve the order parameter (names here should match those in the macros above)
modelResidualsList[1].scalarValueResidual = rnV;
modelResidualsList[1].scalarGradResidual = rnxV;

}

// =================================================================================
// residualLHS (needed only if at least one equation is elliptic)
// =================================================================================
// This function calculates the residual equations for the iterative solver for
// elliptic equations.for each variable. It takes "modelVariablesList" as an input,
// which is a list of the value and derivatives of each of the variables at a
// specific quadrature point. The (x,y,z) location of that quadrature point is given
// by "q_point_loc". The function outputs "modelRes", the value and gradient terms of
// for the left-hand-side of the residual equation for the iterative solver. The
// index for each variable in these lists corresponds to the order it is defined at
// the top of this file (starting at 0), not counting variables that have
// "need_val_LHS", "need_grad_LHS", and "need_hess_LHS" all set to "false". If there
// are multiple elliptic equations, conditional statements should be used to ensure
// that the correct residual is being submitted. The index of the field being solved
// can be accessed by "this->currentFieldIndex".
template <int dim>
void generalizedProblem<dim>::residualLHS(const std::vector<modelVariable<dim>> & modelVariablesList,
		modelResidual<dim> & modelRes,
		dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const {

}

// =================================================================================
// energyDensity (needed only if calcEnergy == true)
// =================================================================================
// This function integrates the free energy density across the computational domain.
// It takes "modelVariablesList" as an input, which is a list of the value and
// derivatives of each of the variables at a specific quadrature point. It also
// takes the mapped quadrature weight, "JxW_value", as an input. The (x,y,z) location
// of the quadrature point is given by "q_point_loc". The weighted value of the
// energy density is added to "energy" variable and the components of the energy
// density are added to the "energy_components" variable (index 0: chemical energy,
// index 1: gradient energy, index 2: elastic energy).
template <int dim>
void generalizedProblem<dim>::energyDensity(const std::vector<modelVariable<dim>> & modelVariablesList,
											const dealii::VectorizedArray<double> & JxW_value,
											dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) {

// The concentration and its derivatives (names here should match those in the macros above)
scalarvalueType c = modelVariablesList[0].scalarValue;
scalargradType cx = modelVariablesList[0].scalarGrad;

// The order parameter and its derivatives (names here should match those in the macros above)
scalarvalueType n = modelVariablesList[1].scalarValue;
scalargradType nx = modelVariablesList[1].scalarGrad;

// The homogenous free energy
scalarvalueType f_chem = (constV(1.0)-hV)*faV + hV*fbV;

// The gradient free energy
scalarvalueType f_grad = constV(0.5*KnV)*nx*nx;

// The total free energy
scalarvalueType total_energy_density;
total_energy_density = f_chem + f_grad;

// Loop to step through each element of the vectorized arrays. Working with deal.ii
// developers to see if there is a more elegant way to do this.
assembler_lock.acquire ();
for (unsigned i=0; i<c.n_array_elements;i++){
  if (c[i] > 1.0e-10){
	  this->energy+=total_energy_density[i]*JxW_value[i];
	  this->energy_components[0]+= f_chem[i]*JxW_value[i];
	  this->energy_components[1]+= f_grad[i]*JxW_value[i];
  }
}
assembler_lock.release ();
}




