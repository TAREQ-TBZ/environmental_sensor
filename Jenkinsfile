pipeline {
    agent any

    stages {

        stage('Clean Workspace') {
            steps {
                cleanWs()
            }
        }

        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Preflight Check') {
            steps {
                script {
                    sh 'ls -al'
                }
            }
        }


        stage('Install Zephyr SDK') {
            agent {
                dockerfile {
                    filename 'Dockerfile'
                    dir '.'
                    additionalBuildArgs '--build-arg USER_UID=1000 --build-arg USER_GID=1000 --build-arg USER_NAME=jenkins'
                }
            }
            steps {
                script {
                    sh '''
                    pwd
                    ls
                    rm -rf ws
                    mkdir -p ws
                    west init -m https://github.com/TAREQ-TBZ/environmental_sensor.git --mr main ws
                    cd ws
                    ls
                    west update

                    pip install -r zephyr/scripts/requirements.txt
                    pip install -r nrf/scripts/requirements.txt
                    pip install -r bootloader/mcuboot/scripts/requirements.txt

                    ls
                    west build -b sham_nrf52833 application/app
                    ls build/zephyr/
                    '''
                }
            }
        }

        stage('Archive Artifacts') {
            steps {
                dir('ws/build/zephyr') {
                    archiveArtifacts artifacts: '*.hex, *.elf', fingerprint: true
                }
            }
        }
    }

    post {
        success {
            echo '✅ Build completed successfully!'
        }
        failure {
            echo '❌ Build failed. Please check the logs.'
        }
    }
}
