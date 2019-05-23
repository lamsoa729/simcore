#ifndef _SIMCORE_STRUCT_ANALYSIS_H_
#define _SIMCORE_STRUCT_ANALYSIS_H_

#include "auxiliary.h"
#include "interaction.h"
#include "object.h"
#include "neighbor_list.h"
#include <mutex>
#include <fftw3.h>

class StructAnalysis {
  private:
    system_parameters * params_;
    std::mutex mtx_;
    int count_,
        n_dim_,
        n_bins_,
        n_bins_1d_,
        local_bins_1d_,
        density_bins_1d_,
        average_structure_,
        n_objs_,
        local_order_analysis_,
        polar_order_analysis_,
        overlap_analysis_,
        density_analysis_,
        * i_step_,
        n_overlaps_,
        n_crossings_init_,
        n_crossings_complete_;

    fftw_complex * density_array_,
                 * fft_array_;
    fftw_plan fft_plan;

    NeighborList crossing_list_;
    double local_bin_width_,
           density_bin_width_;
    float * pdf_array_,
          * nematic_array_,
          * polar_array_,
          * pdf_array_temp_,
          * nematic_array_temp_,
          * polar_array_temp_,
          * structure_array_;

    std::fstream pdf_file_;
    std::fstream nematic_file_;
    std::fstream polar_file_;
    std::fstream structure_file_;
    std::fstream overlap_file_;
    void WriteStructData();
    void WriteDensityData();
    void PopulateLineDensity(double * site1, double * site2);
    void PopulateLineLocal(double * site1, double * site2, double dotprod);
    void BinLine(int x0, int y0, int x1, int y1, double dotprod, bool is_local);
    void BinLineLow(int x0, int y0, int x1, int y1, double dotprod, bool is_local);
    void BinLineHigh(int x0, int y0, int x1, int y1, double dotprod, bool is_local);
    void BinArray(int x, int y, double dotprod, bool is_local);
    void CalculateLocalOrderPair(std::vector<pair_interaction>::iterator pix);
    void CalculatePolarOrderPair(std::vector<pair_interaction>::iterator pix);
    //void CountOverlapEvents(int mid1, int mid2, bool is_overlapping);
    void AddCrossingComplete();
    void AddCrossingInit();
    void AddOverlap();
    void ZeroDensityArray();

  public:
    void Init(system_parameters *params, int *i_step);
    void Clear();
    void CalculateStructurePair(std::vector<pair_interaction>::iterator pix);
    void BinDensity(Object * obj);
    void AverageStructure();
    void SetNumObjs(int nobj);
    int GetNumObjs() {return n_objs_;}
    void IncrementCount() {count_++;}
    void CountOverlap(std::vector<pair_interaction>::iterator pix);
};

#endif
