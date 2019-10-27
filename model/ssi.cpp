
#include <random>
#include <iostream>

using namespace std;

#include "../evppi.hpp"

random_device rd;
mt19937 generator(rd());
normal_distribution<double> dist(0.0, 1.0);

struct Matrix {
    vector < vector<double> > val;
    Matrix(int n, int m) { val.clear(); val.resize(n, vector<double>(m)); }
    int size() { return val.size(); }
    inline vector<double> &operator[](int i) { return val[i]; }
};

Matrix Cholesky(Matrix &A) {
    size_t sz = A.size();
    Matrix Q(sz, sz);
    for (size_t i = 0; i < sz; i++) {
        for (size_t j = 0; j < i; j++) {
            Q[i][j] = A[i][j];
            for (size_t k = 0; k < j; k++)
                Q[i][j] -= Q[i][k] * Q[j][k];
            Q[i][j] /= Q[j][j];
        }
        Q[i][i] = A[i][i];
        for (size_t k = 0; k < i; k++)
            Q[i][i] -= Q[i][k] * Q[i][k];
        Q[i][i] = sqrt(Q[i][i]);
    }
    return Q;
}

vector<double> rand_multinormal(vector<double> &u, Matrix &sigma_cholesky) {
    size_t sz = u.size();
    vector<double> rand(sz);
    vector<double> ret(sz);
    for (size_t i = 0; i < sz; i++) {
        rand[i] = dist(generator);
        for (size_t j = 0; j <= i; j++)
            ret[i] += sigma_cholesky[i][j] * rand[j];
        ret[i] += u[i];
    }
    return ret;
}

int type = 0; // 0: Exposed, 1: Simple, 2: Glue, 3: Advanced
vector <double> dressing_cost(4);

int n_sim = 20000;
int wtp = 20000;
double ssi_qaly_loss = 0.12;

double ssi_cost_mean = 8.972237608;
double ssi_cost_s = 0.163148238;
lognormal_distribution<double> ssi_cost(ssi_cost_mean, ssi_cost_s);

double p_ssi_mean = 0.137984898;
double p_ssi_se = 0.001772214;
normal_distribution<double> p_ssi_dist(p_ssi_mean, p_ssi_se);

vector <double> u(3);
Matrix Sigma(3, 3);
Matrix sigma_cholesky(3, 3);
//p.SSI[,2]<-rnorm(Nsim,meanSIMPLE[type],seSIMPLE[type]) #P(SSI) for simple dressings
//p.SSI[,1]<- expit(logit(p.SSI[,2])+d[,1]) #P(SSI) Exposed
//p.SSI[,3]<- expit(logit(p.SSI[,2])+d[,2]) #P(SSI) Glue
//p.SSI[,4]<- expit(logit(p.SSI[,2])+d[,3]) #P(SSI) Advanced

double expit(double x) {
    return exp(x) / (1 + exp(x));
}

double logit(double x) {
    return log(x / (1 - x));
}

// EVPPI for relative effect を、ExposedとSimpleで比較
// TODO: 評価関数fを2個から任意個に設定できるようにする

void pre_init(PreInfo *info) {
    info->x.resize(3);
}

void post_init(PostInfo *info) {
    info->y.resize(4);
}

void pre_sampling(PreInfo *info) {
    info->x = rand_multinormal(u, sigma_cholesky);
}

void post_sampling(PostInfo *info) {
    info->y[0] = p_ssi_dist(generator);
    info->y[2] = ssi_cost(generator);
    info->y[3] = info->y[2];
}

double f1(EvppiInfo *info) {
    info->post->y[1] = expit(logit(info->post->y[0]) + info->pre->x[0]);

    return -dressing_cost[0] - info->post->y[0] * (info->post->y[2] + ssi_qaly_loss * wtp);
}

double f2(EvppiInfo *info) {
    info->post->y[1] = expit(logit(info->post->y[0]) + info->pre->x[0]);

    return -dressing_cost[1] - info->post->y[1] * (info->post->y[3] + ssi_qaly_loss * wtp);
}

// params for mlmc
int m0 = 2;
int s = 2;
int max_level = 20;
int test_level = 10;

int main() {
    dressing_cost[0] = 0.0;
    dressing_cost[1] = 5.25;
    dressing_cost[2] = 13.86;
    dressing_cost[3] = 21.39;

    u[0] = 0.05021921904305;
    u[1] = -0.06629096195095;
    u[2] = -0.178047360396;

    Sigma[0][0] = 0.06546909;
    Sigma[0][1] = 0.06274766;
    Sigma[0][2] = 0.01781241;
    Sigma[1][0] = 0.06274766;
    Sigma[1][1] = 0.21792037;
    Sigma[1][2] = 0.01727998;
    Sigma[2][0] = 0.01781241;
    Sigma[2][1] = 0.01727998;
    Sigma[2][2] = 0.04631648;

    sigma_cholesky = Cholesky(Sigma);

    MlmcInfo *info = mlmc_init(m0, s, max_level, 1.0, 0.25);
    mlmc_test(info, test_level, n_sim);

    vector <double> eps;
    eps.push_back(0.0002);
    eps.push_back(0.0001);

    mlmc_test_eval_eps(info, eps);

    return 0;
}
