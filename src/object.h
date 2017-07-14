#ifndef _SIMCORE_OBJECT_H_
#define _SIMCORE_OBJECT_H_

#include "auxiliary.h"
#include "interaction.h"

class Object {

  private:
    unsigned int oid_;
    static unsigned int next_oid_;
    static unsigned int next_rid_;

  protected:
    unsigned int cid_;
    unsigned int rid_;
    SID sid_;
    int n_dim_;
    int draw_type_ = 0; // 0 for single color, 1 for orientation color, 2 for don't care
    double position_[3],
           scaled_position_[3],
           prev_position_[3],
           dr_tot_[3], // total distance traveled accumulator for neighbor list
           orientation_[3],
           force_[3],
           torque_[3],
           velocity_[3],
           anglevel_[3],
           color_,
           delta_,
           diameter_,
           length_,
           k_energy_,
           p_energy_,
           kmc_energy_;

    //double *spec_virial_;
    bool is_rigid_ = false;
    bool is_kmc_ = false;
    space_struct *space_;
    graph_struct g_;
    rng_properties rng_;
    virtual void InsertRandom();
    virtual void InsertRandomOriented(double *u);
  public:
    Object(system_parameters *params, space_struct *space, long seed, SID sid);
    bool IsRigid() {return is_rigid_;}
    bool IsKMC() { return is_kmc_; }
    void InitOID() { oid_ = ++next_oid_;}
    void InitCID() { cid_ = oid_;}
    void InitRID() { rid_ = ++next_rid_;}
    virtual void InsertAt(double *pos, double *u);
    void SetPosition(double const *const pos) {
      std::copy(pos, pos+n_dim_, position_);
    }
    void SetScaledPosition(double const *const scaled_pos) {
      std::copy(scaled_pos, scaled_pos+n_dim_, scaled_position_);
    }
    void SetDrTot(double const * const dr_tot) {
      std::copy(dr_tot, dr_tot+n_dim_, dr_tot_);
    }
    void SetOrientation(double const * const u) {
      std::copy(u, u+n_dim_, orientation_);
    }
    void SetVelocity(double const *const v) {
      std::copy(v, v+n_dim_, velocity_);
    }
    void SetPrevPosition(double const * const ppos) {
      std::copy(ppos, ppos+n_dim_, prev_position_);
    }
    void SetDiameter(double new_diameter) {diameter_ = new_diameter;}
    void SetLength(double new_length) {length_ = new_length;}
    void SetSpace(space_struct * space) {space_ = space;}
    void ZeroForce() {
      std::fill(force_,force_+3,0.0);
      std::fill(torque_,torque_+3,0.0);
      p_energy_ = 0.0;
      kmc_energy_ = 0.0;
      //memset(virial_, 0, sizeof(virial_));
    }
    void ZeroDrTot() {
      //printf("[%d] zero dr tot\n", GetOID());
      std::fill(dr_tot_,dr_tot_+3,0.0);
    }
    void AddForce(double const * const f) {
      for (int i=0; i<3; ++i)
        force_[i]+=f[i];
    }
    void SubForce(double const * const f) {
      for (int i=0; i<3; ++i)
        force_[i]-=f[i];
    }
    void SetForce(double const * const f) {
      for (int i=0; i<3; ++i)
        force_[i]=f[i];
    }
    void AddTorque(double const * const t) {
      for (int i=0; i<3; ++i)
        torque_[i]+=t[i];
    }
    void SubTorque(double const * const t) {
      for (int i=0; i<3; ++i)
        torque_[i]-=t[i];
    }
    void SetTorque(double const * const t) {
      for (int i=0; i<3; ++i)
        torque_[i]=t[i];
    }
    void AddPotential(double const p) {p_energy_ += p;}
    void AddKMCEnergy(double const k) {kmc_energy_ += k;}
    void AddForceTorqueEnergy(double const * const F, double const * const T, double const p);
    void AddForceTorqueEnergyKMC(double const * const F, double const * const T, double const p, double const k);
    double const * const GetPosition() {return position_;}
    double const * const GetPrevPosition() {return prev_position_;}
    double const * const GetScaledPosition() {return scaled_position_;}
    double const * const GetVelocity() {return velocity_;}
    virtual double const * const GetDrTot() { return dr_tot_; }
    virtual double GetDrMax() {return 0;}
    virtual double GetDr() {return 0;}
    double const * const GetOrientation() {return orientation_;}
    double const * const GetForce() {return force_;}
    double const * const GetTorque() {return torque_;}
    double const GetDiameter() {return diameter_;}
    double const GetLength() {return length_;}
    double const GetDelta() {return delta_;}
    virtual void Init() {InsertRandom();}
    virtual void Draw(std::vector<graph_struct*> * graph_array);
    virtual void UpdatePeriodic();
    virtual void UpdatePosition() {}
    virtual void UpdatePositionMP() {
      error_exit("ERROR: UpdatePositionMP() needs to be overwritten. Exiting!\n");
    }
    virtual void SetColor(double const c, int dtype) {
      color_ = c;
      draw_type_ = dtype;
    }
    int DrawTypeInt(std::string dt_str);
    virtual void ScalePosition() {
      for (int i=0; i<n_dim_; ++i) {
        position_[i] = 0;
        for (int j=0;j<n_dim_; ++j) {
          position_[i]+=space_->unit_cell[n_dim_*i+j]*scaled_position_[j];
        }
      }
    }
    virtual double const GetKineticEnergy() {return k_energy_;}
    virtual double const GetPotentialEnergy() {return p_energy_;}
    virtual double const GetKMCEnergy() {return kmc_energy_;}
    void SetCID(unsigned int const cid) {cid_=cid;}
    void SetRID(unsigned int const rid) {rid_=rid;}
    unsigned int const GetOID() const {return oid_;}
    unsigned int const GetCID() {return cid_;}
    unsigned int const GetRID() {return rid_;}
    SID const GetSID() {return sid_;}
    virtual void Dump() {
      printf("{%d,%d,%d} -> ", GetOID(), GetRID(), GetCID());
      printf("x(%2.2f, %2.2f), ", GetPosition()[0], GetPosition()[1]);
      printf("f(%2.2f, %2.2f), ", GetForce()[0], GetForce()[1]);
      printf("u(%2.2f), p(%2.2f)\n", GetKineticEnergy(), GetPotentialEnergy());
    }
    virtual int GetCount() {return 0;}

    virtual void WritePosit(std::fstream &oposit);
    virtual void ReadPosit(std::fstream &iposit);
    virtual void WriteSpec(std::fstream &ospec) {}
    virtual void ReadSpec(std::fstream &ispec) {}
    virtual void WriteCheckpoint(std::fstream &ocheck);
    virtual void ReadCheckpoint(std::fstream &icheck);

    void SetRNGState(const std::string& filename) {
      // Load the rng state from binary file
      FILE* pfile = fopen(filename.c_str(), "r");
      auto retval = gsl_rng_fread(pfile, rng_.r);
      if (retval != 0) {
        std::cout << "Reading rng state failed " << retval << std::endl;
      }
    }

};

class Simple : public Object {
  public:
    Simple(system_parameters *params, space_struct *space, long seed, SID sid) :
      Object(params, space, seed, sid) {}
    virtual std::vector<Simple*> GetSimples() {
      std::vector<Simple*> sim_vec;
      sim_vec.push_back(this);
      return sim_vec;
    }
    virtual void AddDr() {
      for (int i=0; i<n_dim_; ++i) {
        dr_tot_[i] += position_[i] - prev_position_[i];
      }
    }
    virtual double GetDr() {
      double dr =0;
      for (int i=0; i<n_dim_; ++i) {
        dr += dr_tot_[i]*dr_tot_[i];
      }
      //printf("[%d] dr2: %2.8f\n", GetOID(), dr);
      return dr;
    }
    virtual double GetDrMax() {
      return GetDr();
    }
    virtual double const GetRigidLength() {return length_;}
    virtual double const GetRigidDiameter() {return diameter_;}
    virtual double const * const GetRigidPosition() {return position_;}
    virtual double const * const GetRigidScaledPosition() {return scaled_position_;}
    virtual double const * const GetRigidOrientation() {return orientation_;}
    virtual void Draw(std::vector<graph_struct*> * graph_array) {
      Object::Draw(graph_array);
    }
    virtual void Dump() {
      printf("{%d,%d,%d} -> ", GetOID(), GetRID(), GetCID());
      printf("x(%2.2f, %2.2f, %2.2f), ", GetPosition()[0], GetPosition()[1], GetPosition()[2]);
      printf("r(%2.2f, %2.2f, %2.2f), ", GetRigidPosition()[0], GetRigidPosition()[1], GetRigidPosition()[2]);
      printf("f(%2.2f, %2.2f, %2.2f), ", GetForce()[0], GetForce()[1], GetForce()[2]);
      printf("t(%2.2f, %2.2f, %2.2f), ", GetTorque()[0], GetTorque()[1], GetTorque()[2]);
      printf("u(%2.2f), p(%2.2f)\n", GetKineticEnergy(), GetPotentialEnergy());
    }
    virtual int GetCount() {return 1;}

};

class Rigid : public Simple {
  protected:
    double rigid_position_[3],
           rigid_scaled_position_[3],
           rigid_orientation_[3],
           rigid_length_,
           rigid_diameter_;
  public:
    Rigid(system_parameters *params, space_struct *space, long seed, SID sid) :
      Simple(params, space, seed, sid) {
        std::fill(rigid_position_,rigid_position_+3,0.0);
        std::fill(rigid_orientation_,rigid_orientation_+3,0.0);
        std::fill(rigid_scaled_position_,rigid_scaled_position_+3,0.0);
        rigid_length_=0;
        rigid_diameter_=1;
        is_rigid_ = true;
    }
    void SetRigidLength(double const len) {rigid_length_=len;}
    void SetRigidDiameter(double const d) {rigid_diameter_=d;}
    void SetRigidPosition(double const * const pos) {std::copy(pos, pos+3,rigid_position_);}
    void SetRigidScaledPosition(double const * const scaled_pos) {std::copy(scaled_pos, scaled_pos+3,rigid_scaled_position_);}
    void SetRigidOrientation(double const * const u) {std::copy(u, u+3, rigid_orientation_);}
    virtual double const GetRigidLength() {return rigid_length_;}
    virtual double const GetRigidDiameter() {return rigid_diameter_;}
    virtual double const * const GetRigidPosition() {return rigid_position_;}
    virtual double const * const GetRigidScaledPosition() {return rigid_scaled_position_;}
    virtual double const * const GetRigidOrientation() {return rigid_orientation_;}
    virtual void Draw(std::vector<graph_struct*> * graph_array) {
      Simple::Draw(graph_array);
    }
};

template<typename...> class Composite;

template <typename T>
class Composite<T> : public Object {
  protected:
    system_parameters *params_;
    std::vector<T> elements_;
    virtual void InitElements(system_parameters *params) {}
  public:
    Composite(system_parameters *params, space_struct *space, long seed, SID sid) : Object(params, space, seed, sid) {params_ = params;} 
    virtual void ZeroForce() {
      std::fill(force_,force_+3,0.0);
      std::fill(torque_,torque_+3,0.0);
      p_energy_ = 0;
      kmc_energy_ = 0;
      for (auto it=elements_.begin(); it!=elements_.end(); ++it) {
        it->ZeroForce();
      }
    }
    virtual std::vector<Simple*> GetSimples() {
      std::vector<Simple*> sim_vec;
      for (auto it=elements_.begin(); it!=elements_.end(); ++it)
        sim_vec.push_back(&(*it));
      return sim_vec;
    }
    virtual double GetDrMax() {
      double dr_max = 0;
      for (auto it=elements_.begin(); it!= elements_.end(); ++it) {
        double dr_mag = it->GetDr();
        if (dr_mag > dr_max)
          dr_max = dr_mag;
      }
      return dr_max;
    }

    //virtual void InitVirial(double *spec_virial){ 
      //for (auto& elem : elements_)
        //elem.InitVirial(spec_virial);
    //}

    virtual void ZeroDrTot() {
      std::fill(dr_tot_,dr_tot_+3,0.0);
      for (auto it=elements_.begin(); it!= elements_.end(); ++it) {
        it->ZeroDrTot();
      }
    }
    virtual void Dump() {
      printf("{%d,%d,%d} -> ", GetOID(), GetRID(), GetCID());
      printf("x(%2.2f, %2.2f), ", GetPosition()[0], GetPosition()[1]);
      printf("f(%2.2f, %2.2f), ", GetForce()[0], GetForce()[1]);
      printf("ke(%2.2f), pe(%2.2f)\n", GetKineticEnergy(), GetPotentialEnergy());
      for (auto it=elements_.begin(); it!=elements_.end(); ++it) {
        it->Dump();
      }
    }

    virtual int GetCount() {
      return elements_.size();
    }

    virtual void WritePosit(std::fstream &op){
      int size;
      size = elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(int));

      for (auto& elem : elements_)
        elem.WritePosit(op);
    }

    virtual void ReadPosit(std::fstream &ip){
      if (ip.eof()) return;
      int size;
      T elmt(params_, space_, params_->seed, sid_);
      ip.read(reinterpret_cast<char*>(&size), sizeof(int));
      elements_.resize(size, elmt);

      for (auto& elem : elements_)
        elem.ReadPosit(ip);
    }

    virtual void WriteSpec(std::fstream &os){
      int size;
      size = elements_.size();
      os.write(reinterpret_cast<char*>(&size), sizeof(int));

      for (auto& elem : elements_)
        elem.WriteSpec(os);
    }

    virtual void ReadSpec(std::fstream &ip){
      if (ip.eof()) return;
      int size;
      T elmt(params_, space_, params_->seed, sid_);
      ip.read(reinterpret_cast<char*>(&size), sizeof(int));
      elements_.resize(size, elmt);

      for (auto& elem : elements_)
        elem.ReadSpec(ip);
    }

    virtual void WriteCheckpoint(std::fstream &oss){
      int size;
      size = elements_.size();
      oss.write(reinterpret_cast<char*>(&size), sizeof(int));

      for (auto& elem : elements_)
        elem.WriteCheckpoint(oss);
    }

    virtual void ReadCheckpoint(std::fstream &iss){
      if (iss.eof()) return;
      int size;
      T elmt(params_, space_, params_->seed, sid_);
      iss.read(reinterpret_cast<char*>(&size), sizeof(int));
      elements_.resize(size, elmt);

      for (auto& elem : elements_)
        elem.ReadCheckpoint(iss);
    }

    virtual void ScalePosition() {
      for (auto elem : elements_)
        elem.ScalePosition();
    }
};

template <typename T, typename V>
class Composite<T,V> : public Object {
  protected:
    system_parameters *params_;
    std::vector<T> elements_;
    std::vector<V> v_elements_;
  public:
    Composite(system_parameters *params, space_struct *space, long seed, SID sid) : Object(params, space, seed, sid) {params_ = params;}  
    virtual void ZeroForce() {
      std::fill(force_,force_+3,0.0);
      std::fill(torque_,torque_+3,0.0);
      p_energy_ = 0;
      kmc_energy_ = 0;
      for (auto it=elements_.begin(); it!=elements_.end(); ++it) {
        it->ZeroForce();
      }
      for (auto it=v_elements_.begin(); it!=v_elements_.end(); ++it) {
        it->ZeroForce();
      }
    }
    virtual std::vector<Simple*> GetSimples() {
      std::vector<Simple*> sim_vec;
      for (auto it=v_elements_.begin(); it!=v_elements_.end(); ++it)
        sim_vec.push_back(&(*it));
      return sim_vec;
    }
    virtual void ZeroDrTot() {
      std::fill(dr_tot_,dr_tot_+3,0.0);
      for (auto it=elements_.begin(); it!= elements_.end(); ++it) {
        it->ZeroDrTot();
      }
    }
    virtual void Dump() {
      printf("{%d,%d,%d} -> ", GetOID(), GetRID(), GetCID());
      printf("x(%2.2f, %2.2f), ", GetPosition()[0], GetPosition()[1]);
      printf("f(%2.4f, %2.4f), ", GetForce()[0], GetForce()[1]);
      printf("t(%2.4f, %2.4f), ", GetTorque()[0], GetTorque()[1]);
      printf("ke(%2.2f), pe(%2.2f)\n", GetKineticEnergy(), GetPotentialEnergy());
      for (auto it=elements_.begin(); it!=elements_.end(); ++it) {
        it->Dump();
      }
    }
    virtual int GetCount() {
      return v_elements_.size();
    }
    virtual double GetDrMax() {
      double dr_max = 0;
      for (auto it=elements_.begin(); it!= elements_.end(); ++it) {
        double dr_mag = it->GetDr();
        if (dr_mag > dr_max)
          dr_max = dr_mag;
      }
      return dr_max;
    }

    virtual void ReadPosit(std::fstream &ip){
      int size;
      T elmt(params_, space_, params_->seed, sid_);
      V v_elmt(params_, space_, params_->seed, sid_);

      ip.read(reinterpret_cast<char*>(&size), sizeof(size));
      elements_.resize(size, elmt);
      ip.read(reinterpret_cast<char*>(&size), sizeof(size));
      v_elements_.resize(size, v_elmt);


      for (auto& elem : elements_)
        elem.ReadPosit(ip);
      for (auto& velem : v_elements_)
        velem.ReadPosit(ip);
    }

    virtual void WritePosit(std::fstream &op){
      int size;
      size = elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(size));
      size = v_elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(size));

      for (auto& elem : elements_)
        elem.WritePosit(op);
      for (auto& velem : v_elements_)
        velem.WritePosit(op);
    }

    virtual void ReadSpec(std::fstream &ip){
      int size;
      T elmt(params_, space_, params_->seed, sid_);
      V v_elmt(params_, space_, params_->seed, sid_);

      ip.read(reinterpret_cast<char*>(&size), sizeof(size));
      elements_.resize(size, elmt);
      ip.read(reinterpret_cast<char*>(&size), sizeof(size));
      v_elements_.resize(size, v_elmt);


      for (auto& elem : elements_)
        elem.ReadSpec(ip);
      for (auto& velem : v_elements_)
        velem.ReadSpec(ip);
    }

    virtual void WriteSpec(std::fstream &op){
      int size;
      size = elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(size));
      size = v_elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(size));

      for (auto& elem : elements_)
        elem.WriteSpec(op);
      for (auto& velem : v_elements_)
        velem.WriteSpec(op);
    }

    virtual void ReadCheckpoint(std::fstream &ip){
      int size;
      T elmt(params_, space_, params_->seed, sid_);
      V v_elmt(params_, space_, params_->seed, sid_);

      ip.read(reinterpret_cast<char*>(&size), sizeof(size));
      elements_.resize(size, elmt);
      ip.read(reinterpret_cast<char*>(&size), sizeof(size));
      v_elements_.resize(size, v_elmt);


      for (auto& elem : elements_)
        elem.ReadCheckpoint(ip);
      for (auto& velem : v_elements_)
        velem.ReadCheckpoint(ip);
    }

    virtual void WriteCheckpoint(std::fstream &op){
      int size;
      size = elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(size));
      size = v_elements_.size();
      op.write(reinterpret_cast<char*>(&size), sizeof(size));

      for (auto& elem : elements_)
        elem.WriteCheckpoint(op);
      for (auto& velem : v_elements_)
        velem.WriteCheckpoint(op);
    }

    virtual void ScalePosition() {
      for (auto elem : elements_)
        elem.ScalePosition();
      for (auto v_elem : v_elements_)
        v_elem.ScalePosition();
    }
};

void MinimumDistance(Simple* o1, Simple* o2, Interaction *ix, space_struct *space); 

#endif // _SIMCORE_OBJECT_H_
