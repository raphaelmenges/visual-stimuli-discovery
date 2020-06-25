import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.preprocessing import MinMaxScaler
from sklearn.metrics import f1_score
from sklearn import svm
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import RandomForestClassifier
from imblearn.over_sampling import SMOTE

# Settings
scaler = 'minmax' # or standard
baseline_step_count = 100 # how many tresholds are evaluated to find the optimum

# Class of abstract classifier. Performs preprocessing, too
class Classifier:
    
    # Use machine to predict labels of test data
    def apply(self, X_train, y_train, X_test, idx_baseline):
        
        # Resample training data
        sm = SMOTE(random_state=42, sampling_strategy='not majority')
        X_res, y_res = sm.fit_resample(X_train, y_train)

        # Print infos about training data
        # print("X_train" + str(X_train.shape) + " y_train" + str(y_train.shape) + " Visual Changes: " + str(np.count_nonzero(y_train)))
        # print("X_res" + str(X_res.shape) + " y_res" + str(y_res.shape) + " Visual Changes: " + str(np.count_nonzero(y_res)))
        
        # Normalize dataset on the basis of the training data
        if scaler == 'standard':
            self.scaler = StandardScaler()
        else:
            self.scaler = MinMaxScaler()
        self.scaler.fit(X_res)
        X_res = self.scaler.transform(X_res)
        
        # Logistic Regression
        self.clf_logreg = LogisticRegression(
            random_state=42,
            solver='liblinear',
            multi_class='ovr', # binary data
            class_weight=None) # 'balanced'
        self.clf_logreg.fit(X_res, y_res)
        
        # Support Vector Machine
        self.clf_svc = svm.SVC(
            random_state=42,
            kernel='rbf',
            gamma='scale',
            decision_function_shape='ovr', 
            class_weight=None) # 'balanced'
        self.clf_svc.fit(X_res, y_res)
        
        # Random Forest
        self.clf_forest = RandomForestClassifier(
            random_state=42,
            n_estimators=100,
            class_weight=None, # 'balanced', 'balanced_subsample'
            criterion='entropy', # 'gini'
            max_depth=None,
            min_samples_split=2,
            min_samples_leaf=1)
        self.clf_forest.fit(X_res, y_res)
        
        # Baseline
        base_values = X_res[:,idx_baseline]
        min_base_value = np.min(base_values)
        max_base_value = np.max(base_values)
        step_base = (max_base_value - min_base_value) / baseline_step_count
        
        # Iterate over possible threshold and find optimal threshold
        best_thresh = min_base_value
        best_score = 0.0
        for i in range(0,baseline_step_count):
            pred_thresh = min_base_value + (step_base * i) 
            pred_base = [int(v > pred_thresh) for v in base_values]
            pred_score = f1_score(y_res, pred_base, average='weighted') 
            if pred_score > best_score:
                best_thresh = pred_thresh
                best_score = pred_score
                
        # Store predictions
        X_test = self.scaler.transform(X_test)
        pred = {
            'logreg'    : self.clf_logreg.predict(X_test).astype(int),
            'svc'       : self.clf_svc.predict(X_test).astype(int),
            'forest'    : self.clf_forest.predict(X_test).astype(int),
            'baseline'  : [int(v > best_thresh) for v in X_test[:,idx_baseline]],
            'importance': self.clf_forest.feature_importances_}
        
        # Return predictions
        return pred