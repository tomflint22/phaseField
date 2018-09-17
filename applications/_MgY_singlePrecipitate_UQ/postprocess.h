// =================================================================================
// Set the attributes of the primary field variables
// =================================================================================
void variableAttributeLoader::loadPostProcessorVariableAttributes(){
	// Variable 0
	set_variable_name				(0,"f_tot");
	set_variable_type				(0,SCALAR);

	set_need_value_residual_term	(0,true);
	set_need_gradient_residual_term	(0,false);

    set_output_integral         	(0,true);

	// Variable 1
	set_variable_name				(1,"mu_c");
	set_variable_type				(1,SCALAR);

	set_need_value_residual_term	(1,true);
	set_need_gradient_residual_term	(1,false);

    set_output_integral         	(1,false);


	// Variable 2
	set_variable_name				(2,"von_mises_stress");
	set_variable_type				(2,SCALAR);

	set_need_value_residual_term	(2,true);
	set_need_gradient_residual_term	(2,false);
	set_output_integral         	(2,false);

	// Variable 3
	set_variable_name				(3,"grad_mu_c");
	set_variable_type				(3,SCALAR);

	set_need_value_residual_term	(3,true);
	set_need_gradient_residual_term	(3,false);
	set_output_integral         	(3,false);

	// Variable 3
	set_variable_name				(4,"grad_mu_c_el");
	set_variable_type				(4,SCALAR);

	set_need_value_residual_term	(4,true);
	set_need_gradient_residual_term	(4,false);
	set_output_integral         	(4,false);

	// Variable 3
	set_variable_name				(5,"dc_dt_el");
	set_variable_type				(5,SCALAR);

	set_need_value_residual_term	(5,false);
	set_need_gradient_residual_term	(5,true);
	set_output_integral         	(5,false);

	// Variable 3
	set_variable_name				(6,"dc_dt_chem");
	set_variable_type				(6,SCALAR);

	set_need_value_residual_term	(6,false);
	set_need_gradient_residual_term	(6,true);
	set_output_integral         	(6,false);

	// Variable 1
	set_variable_name				(7,"dn_dt_chem");
	set_variable_type				(7,SCALAR);

	set_need_value_residual_term	(7,true);
	set_need_gradient_residual_term	(7,false);

    set_output_integral         	(7,false);

	// Variable 1
	set_variable_name				(8,"dn_dt_el");
	set_variable_type				(8,SCALAR);

	set_need_value_residual_term	(8,true);
	set_need_gradient_residual_term	(8,false);

    set_output_integral         	(8,false);

	// Variable 1
	set_variable_name				(9,"dn_dt_el_t1");
	set_variable_type				(9,SCALAR);

	set_need_value_residual_term	(9,true);
	set_need_gradient_residual_term	(9,false);

    set_output_integral         	(9,false);
	// Variable 1
	set_variable_name				(10,"dn_dt_el_t2");
	set_variable_type				(10,SCALAR);

	set_need_value_residual_term	(10,true);
	set_need_gradient_residual_term	(10,false);

    set_output_integral         	(10,false);


}

// =================================================================================

template <int dim,int degree>
void customPDE<dim,degree>::postProcessedFields(const variableContainer<dim,degree,dealii::VectorizedArray<double> > & variable_list,
	variableContainer<dim,degree,dealii::VectorizedArray<double> > & pp_variable_list,
	const dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const {

		scalarvalueType total_energy_density = constV(0.0);

		/// The concentration and its derivatives (names here should match those in the macros above)
		scalarvalueType c = variable_list.get_scalar_value(0);
		scalargradType cx = variable_list.get_scalar_gradient(0);

		// The first order parameter and its derivatives (names here should match those in the macros above)
		scalarvalueType n1 = variable_list.get_scalar_value(1);
		scalargradType n1x = variable_list.get_scalar_gradient(1);

		// The derivative of the displacement vector (names here should match those in the macros above)
		vectorgradType ux = variable_list.get_vector_gradient(2);

		vectorhessType uxx;

		if (c_dependent_misfit == true){
			uxx = variable_list.get_vector_hessian(2);
		}

		scalarvalueType f_chem = (constV(1.0)-(h1V))*faV + (h1V)*fbV + constV(W)*fbarrierV;

		scalarvalueType f_grad = constV(0.0);

		for (int i=0; i<dim; i++){
			for (int j=0; j<dim; j++){
				f_grad += constV(0.5*Kn1[i][j])*n1x[i]*n1x[j];
			}
		}

		// Calculate the derivatives of c_beta (derivatives of c_alpha aren't needed)
		scalarvalueType cbnV, cbcV, cbcnV,cacV;

		cbcV = faccV/( (constV(1.0)-h1V)*fbccV + h1V*faccV );
		cacV = fbccV/( (constV(1.0)-h1V)*fbccV + h1V*faccV );
		cbnV = hn1V * (c_alpha - c_beta) * cbcV;
		cbcnV = (faccV * (fbccV-faccV) * hn1V)/( ((1.0-h1V)*fbccV + h1V*faccV)*((1.0-h1V)*fbccV + h1V*faccV) );  // Note: this is only true if faV and fbV are quadratic

		// Calculate the stress-free transformation strain and its derivatives at the quadrature point
		dealii::Tensor<2, dim, dealii::VectorizedArray<double> > sfts1, sfts1c, sfts1cc, sfts1n, sfts1cn;

		for (unsigned int i=0; i<dim; i++){
		for (unsigned int j=0; j<dim; j++){
			// Polynomial fits for the stress-free transformation strains, of the form: sfts = a_p * c_beta + b_p
			sfts1[i][j] = constV(sfts_linear1[i][j])*c_beta + constV(sfts_const1[i][j]);
			sfts1c[i][j] = constV(sfts_linear1[i][j]) * cbcV;
			sfts1cc[i][j] = constV(0.0);
			sfts1n[i][j] = constV(sfts_linear1[i][j]) * cbnV;
			sfts1cn[i][j] = constV(sfts_linear1[i][j]) * cbcnV;
		}
		}

		//compute E2=(E-E0)
		dealii::VectorizedArray<double> E2[dim][dim], S[dim][dim];

		for (unsigned int i=0; i<dim; i++){
			for (unsigned int j=0; j<dim; j++){
				E2[i][j]= constV(0.5)*(ux[i][j]+ux[j][i])-( sfts1[i][j]*h1V);

			}
		}

		//compute stress
		//S=C*(E-E0)
		dealii::VectorizedArray<double> CIJ_combined[2*dim-1+dim/3][2*dim-1+dim/3];

		if (n_dependent_stiffness == true){
			for (unsigned int i=0; i<2*dim-1+dim/3; i++){
				for (unsigned int j=0; j<2*dim-1+dim/3; j++){
					CIJ_combined[i][j] = CIJ_Mg[i][j]*(constV(1.0)-h1V) + CIJ_Beta[i][j]*h1V;
				}
			}
			computeStress<dim>(CIJ_combined, E2, S);
		}
		else{
			computeStress<dim>(CIJ_Mg, E2, S);
		}

		scalarvalueType f_el = constV(0.0);

		for (unsigned int i=0; i<dim; i++){
			for (unsigned int j=0; j<dim; j++){
				f_el += constV(0.5) * S[i][j]*E2[i][j];
			}
		}

		total_energy_density = f_chem + f_grad + f_el;

		// Calculate the chemical potential for the concentration
		scalarvalueType mu_c = constV(0.0);
		mu_c += facV*cacV * (constV(1.0)-h1V) + fbcV*cbcV * h1V;
		for (unsigned int i=0; i<dim; i++){
			for (unsigned int j=0; j<dim; j++){
				mu_c -= S[i][j]*( sfts1c[i][j]*h1V);
			}
		}

		scalarvalueType mu_c_el = constV(0.0);
		for (unsigned int i=0; i<dim; i++){
			for (unsigned int j=0; j<dim; j++){
				mu_c_el -= S[i][j]*( sfts1c[i][j]*h1V);
			}
		}

		// Calculate the chemical potential for the concentration
		scalargradType grad_mu_c;

		// compute the stress term in the gradient of the concentration chemical potential, grad_mu_el = -[C*(E-E0)*E0c]x, must be a vector with length dim
		scalargradType grad_mu_c_el, grad_mu_el;

		dealii::VectorizedArray<double> S2[dim][dim];

		if (n_dependent_stiffness == true){
			computeStress<dim>(CIJ_Beta-CIJ_Mg, E2, S2);
		}

		grad_mu_el = constV(0.0);
		if (c_dependent_misfit == true){
			dealii::VectorizedArray<double> E3[dim][dim], S3[dim][dim];

			for (unsigned int i=0; i<dim; i++){
				for (unsigned int j=0; j<dim; j++){
					E3[i][j] =  -( sfts1c[i][j]*h1V );
				}
			}

			if (n_dependent_stiffness == true){
				computeStress<dim>(CIJ_combined, E3, S3);
			}
			else{
				computeStress<dim>(CIJ_Mg, E3, S3);
			}

			for (unsigned int i=0; i<dim; i++){
				for (unsigned int j=0; j<dim; j++){
					for (unsigned int k=0; k<dim; k++){
						grad_mu_el[k] += S3[i][j] * (constV(0.5)*(uxx[i][j][k]+uxx[j][i][k]) + E3[i][j]*cx[k]
																  - ( (sfts1[i][j]*hn1V + sfts1n[i][j]*h1V) *n1x[k]) );

						//grad_mu_el[k]+= - S[i][j] * ( (sfts1c[i][j]*hn1V + h1V*sfts1cn[i][j])*n1x[k] + ( sfts1cc[i][j]*h1V )*cx[k]);

						if (n_dependent_stiffness == true){
							grad_mu_el[k]+= S2[i][j] * (hn1V*n1x[k]) * (E3[i][j]);

						}
					}
				}
			}
		}

		grad_mu_c = cx + n1x*(c_alpha-c_beta)*hn1V + grad_mu_el * (h1V*faccV+(constV(1.0)-h1V)*fbccV)/constV(faccV*fbccV);


// The Von Mises Stress
dealii::VectorizedArray<double> vm_stress;
if (dim == 3){
    vm_stress = (S[0][0]-S[1][1])*(S[0][0]-S[1][1]) + (S[1][1]-S[2][2])*(S[1][1]-S[2][2]) + (S[2][2]-S[0][0])*(S[2][2]-S[0][0]);
    vm_stress += constV(6.0)*(S[0][1]*S[0][1] + S[1][2]*S[1][2] + S[2][0]*S[2][0]);
    vm_stress *= constV(0.5);
    vm_stress = std::sqrt(vm_stress);
}
else {
    vm_stress = S[0][0]*S[0][0] - S[0][0]*S[1][1] + S[1][1]*S[1][1] + constV(3.0)*S[0][1]*S[0][1];
    vm_stress = std::sqrt(vm_stress);
}

scalargradType dc_dt_chem, dc_dt_el;

dc_dt_chem = constV(-userInputs.dtValue)*McV* (cx + n1x*(c_alpha-c_beta)*hn1V);
dc_dt_el = constV(-userInputs.dtValue)*McV* (grad_mu_el * (h1V*faccV+(constV(1.0)-h1V)*fbccV)/constV(faccV*fbccV) );

scalarvalueType dn_dt_chem, dn_dt_el;

// Compute one of the stress terms in the order parameter chemical potential, nDependentMisfitACp = -C*(E-E0)*(E0_n)
dealii::VectorizedArray<double> nDependentMisfitAC1=constV(0.0);
dealii::VectorizedArray<double> nDependentMisfitAC1_t1=constV(0.0);
dealii::VectorizedArray<double> nDependentMisfitAC1_t2=constV(0.0);

for (unsigned int i=0; i<dim; i++){
for (unsigned int j=0; j<dim; j++){
	  nDependentMisfitAC1+=-S[i][j]*(sfts1n[i][j]*h1V + sfts1[i][j]*hn1V);
	  nDependentMisfitAC1_t1+=-S[i][j]*(sfts1n[i][j]*h1V);
	  nDependentMisfitAC1_t2+=-S[i][j]*(sfts1[i][j]*hn1V);
}
}


// Compute the other stress term in the order parameter chemical potential, heterMechACp = 0.5*Hn*(C_beta-C_alpha)*(E-E0)*(E-E0)
dealii::VectorizedArray<double> heterMechAC1=constV(0.0);

if (n_dependent_stiffness == true){
	computeStress<dim>(CIJ_Beta-CIJ_Mg, E2, S2);

	for (unsigned int i=0; i<dim; i++){
		for (unsigned int j=0; j<dim; j++){
			heterMechAC1 += S2[i][j]*E2[i][j];
		}
	}
	heterMechAC1 = 0.5*hn1V*heterMechAC1;
}

dn_dt_chem = -constV(userInputs.dtValue*Mn1V)*( (fbV-faV)*hn1V - (c_beta-c_alpha)*facV*hn1V + W*fbarriernV);
dn_dt_el = -constV(userInputs.dtValue*Mn1V)*( nDependentMisfitAC1 + heterMechAC1);

scalarvalueType dn_dt_el_t1, dn_dt_el_t2;
dn_dt_el_t1 = -constV(userInputs.dtValue*Mn1V)*( nDependentMisfitAC1_t1);
dn_dt_el_t2 = -constV(userInputs.dtValue*Mn1V)*( nDependentMisfitAC1_t2);

// Residuals for the equation to evolve the order parameter (names here should match those in the macros above)
pp_variable_list.set_scalar_value_residual_term(0, total_energy_density);
pp_variable_list.set_scalar_value_residual_term(1, mu_c);
pp_variable_list.set_scalar_value_residual_term(2, vm_stress);
pp_variable_list.set_scalar_value_residual_term(3, std::sqrt(grad_mu_c.norm_square()));
pp_variable_list.set_scalar_value_residual_term(4, std::sqrt(grad_mu_el.norm_square()) * (h1V*faccV+(constV(1.0)-h1V)*fbccV)/constV(faccV*fbccV));
pp_variable_list.set_scalar_gradient_residual_term(5, dc_dt_el);
pp_variable_list.set_scalar_gradient_residual_term(6, dc_dt_chem);

pp_variable_list.set_scalar_value_residual_term(7, dn_dt_chem);
pp_variable_list.set_scalar_value_residual_term(8, dn_dt_el);
pp_variable_list.set_scalar_value_residual_term(9, dn_dt_el_t1);
pp_variable_list.set_scalar_value_residual_term(10, dn_dt_el_t2);

}