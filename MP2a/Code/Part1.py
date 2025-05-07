'''
Name: Justin Ngo
PSU Email: jvn5439
PSUID: 933902868
Class: CMPEN 462
Assignment: Mini Project 2a
Part: Part 1 - Classifier Comparison
Date: 04.13.25
'''

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.linear_model import LogisticRegression
from sklearn.svm import SVC
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, classification_report


# Step 2: Load the datasets
train_data = pd.read_csv('D:/Work Stuff/CODEE/CMPSC-462/MP2a/Code/train_kdd_small.csv')
test_data = pd.read_csv('D:/Work Stuff/CODEE/CMPSC-462/MP2a/Code//test_kdd_small.csv')

# Step 3: Preprocess the data

# Identify numeric and categorical columns
# Note: In the KDD99 dataset, the last column typically contains the label
feature_columns = train_data.columns[:-1]  # All columns except the last one
label_column = train_data.columns[-1]      # Last column contains the label

print(f"Label column: {label_column}")

# Create binary labels (normal vs attack)
# For KDD99, typically we're distinguishing 'normal' from various attacks
train_labels = train_data[label_column].copy()
test_labels = test_data[label_column].copy()

# Create binary classification (normal vs attack)
train_labels_binary = train_labels.apply(lambda x: 0 if x == 'normal' else 1)
test_labels_binary = test_labels.apply(lambda x: 0 if x == 'normal' else 1)

print(f"\nUnique classes in training data: {train_labels.unique()}")
print(f"After binary transformation: {train_labels_binary.unique()}")

# Handle categorical features
# First, let's identify categorical columns
categorical_cols = train_data.select_dtypes(include=['object']).columns
print(f"\nCategorical columns: {categorical_cols}")

# Apply one-hot encoding for categorical features
train_data_encoded = pd.get_dummies(train_data.drop(label_column, axis=1))
test_data_encoded = pd.get_dummies(test_data.drop(label_column, axis=1))

# Make sure training and test datasets have the same columns
# Some categories might appear in test but not in train or vice versa
all_columns = list(set(train_data_encoded.columns) | set(test_data_encoded.columns))
train_data_encoded = train_data_encoded.reindex(columns=all_columns, fill_value=0)
test_data_encoded = test_data_encoded.reindex(columns=all_columns, fill_value=0)

# Scale numerical features for better performance
scaler = StandardScaler()
train_features = scaler.fit_transform(train_data_encoded)
test_features = scaler.transform(test_data_encoded)

print(f"Preprocessed training features shape: {train_features.shape}")
print(f"Preprocessed test features shape: {test_features.shape}")

# Step 4: Implement classifiers
print("\nStep 4: Implementing classifiers")

# Function to evaluate and print metrics
def evaluate_model(model, X_train, y_train, X_test, y_test, model_name):
    # Train the model
    print(f"\nTraining {model_name}...")
    model.fit(X_train, y_train)
    
    # Make predictions
    y_pred = model.predict(X_test)
    
    # Calculate metrics
    accuracy = accuracy_score(y_test, y_pred)
    precision = precision_score(y_test, y_pred)
    recall = recall_score(y_test, y_pred)
    f1 = f1_score(y_test, y_pred)
    
    # Print metrics
    print(f"{model_name} Performance:")
    print(f"Accuracy: {accuracy:.4f}")
    print(f"Precision: {precision:.4f}")
    print(f"Recall: {recall:.4f}")
    print(f"F1 Score: {f1:.4f}")
    
    # Print classification report for more details
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=['normal', 'attack']))
    
    return model, accuracy, precision, recall, f1

# 4.1 Logistic Regression
print("\nImplementing Logistic Regression")
log_reg = LogisticRegression(max_iter=1000, random_state=42)
log_reg_results = evaluate_model(log_reg, train_features, train_labels_binary, test_features, test_labels_binary, "Logistic Regression")

# 4.2 Support Vector Machine
print("\nImplementing Support Vector Machine")
svm = SVC(kernel='rbf', random_state=42)
svm_results = evaluate_model(svm, train_features, train_labels_binary, test_features, test_labels_binary, "Support Vector Machine")

# 4.3 Random Forest
print("\nImplementing Random Forest")
rf = RandomForestClassifier(n_estimators=100, random_state=42)
rf_results = evaluate_model(rf, train_features, train_labels_binary, test_features, test_labels_binary, "Random Forest")

# Step 5: Compare results
print("\nStep 5: Comparing classifier results")
models = ["Logistic Regression", "Support Vector Machine", "Random Forest"]
accuracies = [log_reg_results[1], svm_results[1], rf_results[1]]
precisions = [log_reg_results[2], svm_results[2], rf_results[2]]
recalls = [log_reg_results[3], svm_results[3], rf_results[3]]
f1_scores = [log_reg_results[4], svm_results[4], rf_results[4]]

results_df = pd.DataFrame({
    'Model': models,
    'Accuracy': accuracies,
    'Precision': precisions,
    'Recall': recalls,
    'F1 Score': f1_scores
})

print("\nComparison of Models:")
print(results_df)

# Find the best model based on F1 score
best_model_index = np.argmax(f1_scores)
print(f"\nBest Model (based on F1 Score): {models[best_model_index]}")
print(f"F1 Score: {f1_scores[best_model_index]:.4f}")

# Plotting the results
metrics = ['Accuracy', 'Precision', 'Recall', 'F1 Score']
x = np.arange(len(models))  # the label locations

# Create subplots for each metric
fig, axes = plt.subplots(2, 2, figsize=(12, 10))
axes = axes.flatten()

for i, metric in enumerate(metrics):
    values = results_df[metric]
    axes[i].bar(x, values, color=['blue', 'orange', 'green'])
    axes[i].set_title(f'{metric} Comparison')
    axes[i].set_xticks(x)
    axes[i].set_xticklabels(models, rotation=45)
    axes[i].set_ylabel(metric)
    axes[i].set_ylim(0, 1)  # Metrics are between 0 and 1
    for j, v in enumerate(values):
        axes[i].text(j, v + 0.02, f"{v:.2f}", ha='center', fontsize=10)

# Adjust layout and show the plot
plt.tight_layout()
plt.show()